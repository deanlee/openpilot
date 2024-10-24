#include "tools/replay/replay.h"

#include <capnp/dynamic.h>
#include <csignal>
#include "cereal/services.h"
#include "common/params.h"
#include "tools/replay/util.h"

static void interrupt_sleep_handler(int signal) {}

// Helper function to notify events with safety checks
template <typename Callback, typename... Args>
void notifyEvent(Callback &callback, Args &&...args) {
  if (callback) {
    callback(std::forward<Args>(args)...);
  }
}

Replay::Replay(const std::string &route, std::vector<std::string> allow, std::vector<std::string> block,
               SubMaster *sm, uint32_t flags, const std::string &data_dir)
    : sm_(sm), flags_(flags), seg_mgr_(route, data_dir) {

  std::signal(SIGUSR1, interrupt_sleep_handler); // Signal handler for SIGUSR1

  if (!(flags_ & REPLAY_FLAG_ALL_SERVICES)) {
    block.insert(block.end(), {"uiDebug", "userFlag"});
  }

  initializeSockets(allow, block);
  initializeSegmentManager(allow);
}

Replay::~Replay() {
  stop();
}

void Replay::initializeSockets(const std::vector<std::string> &allow, const std::vector<std::string> &block) {
  auto event_schema = capnp::Schema::from<cereal::Event>().asStruct();
  sockets_.resize(event_schema.getUnionFields().size());

  std::vector<const char *> active_services;
  for (const auto &[name, _] : services) {
    bool in_block = std::find(block.begin(), block.end(), name) != block.end();
    bool in_allow = std::find(allow.begin(), allow.end(), name) != allow.end();

    if (!in_block && (allow.empty() || in_allow)) {
      uint16_t which = event_schema.getFieldByName(name).getProto().getDiscriminantValue();
      sockets_[which] = name.c_str();
      active_services.push_back(name.c_str());
    }
  }

  rInfo("active services: %s", join(active_services, ", ").c_str());
  if (!sm_) {
    pm_ = std::make_unique<PubMaster>(active_services);
  }
}

void Replay::initializeSegmentManager(const std::vector<std::string> &allow) {
  seg_mgr_.setFlags(flags_);
  seg_mgr_.setCallback([this]() { onSegmentMerged(); });

  if (!allow.empty()) {
    std::vector<bool> filters(sockets_.size(), false);
    for (size_t i = 0; i < sockets_.size(); ++i) {
      filters[i] = (i == cereal::Event::Which::INIT_DATA || i == cereal::Event::Which::CAR_PARAMS || sockets_[i]);
    }
    seg_mgr_.setFilters(filters);
  }
}

void Replay::stop() {
  exit_ = true;
  if (stream_thread_.joinable()) {
    rInfo("shutdown: in progress...");
    pauseStreamThread();
    stream_cv_.notify_one();
    stream_thread_.join();
    rInfo("shutdown: done");
  }
  camera_server_.reset();
}

bool Replay::load() {
  rInfo("loading route %s", seg_mgr_.route_.name().c_str());
  if (!seg_mgr_.load()) return false;

  min_seconds_ = seg_mgr_.route_.segments().begin()->first * 60;
  max_seconds_ = (seg_mgr_.route_.segments().rbegin()->first + 1) * 60;
  return true;
}

void Replay::start(int seconds) {
  seekTo(min_seconds_ + seconds, false);
}

void Replay::updateEvents(const std::function<bool()> &update_events_function) {
  pauseStreamThread();
  {
    std::unique_lock lk(stream_lock_);
    events_ready_ = update_events_function();
    paused_ = user_paused_;
  }
  stream_cv_.notify_one();
}

void Replay::seekTo(double seconds, bool relative) {
  updateEvents([=]() {
    double target_time = relative ? seconds + currentSeconds() : seconds;
    target_time = std::max(0.0, target_time);
    int target_segment = target_time / 60;
    if (!seg_mgr_.hasSegment(target_segment)) {
      rWarning("Invalid seek to %.2f s (segment %d)", target_time, target_segment);
      return true;
    }

    rInfo("Seeking to %.2f s (segment %d)", target_time, target_segment);
    current_segment_ = target_segment;
    cur_mono_time_ = route_start_ts_ + target_time * 1e9;
    seeking_to_ = target_time;
    return false;
  });

  checkSeekProgress();
  seg_mgr_.setCurrentSegment(current_segment_);
}

void Replay::seekToFlag(FindFlag flag) {
  if (auto next = timeline_.find(currentSeconds(), flag)) {
    seekTo(*next - 2, false);  // Seek to 2 seconds before flag
  }
}

void Replay::checkSeekProgress() {
  if (seeking_to_ < 0) return;

  if (isSegmentLoaded(seeking_to_ / 60)) {
    notifyEvent(onSeekedTo, seeking_to_);
    seeking_to_ = -1;
    updateEvents([]() { return true; });
  } else {
    notifyEvent(onSeeking, seeking_to_);
  }
}

void Replay::pause(bool pause) {
  if (user_paused_ != pause) {
    pauseStreamThread();
    {
      std::unique_lock lock(stream_lock_);
      rWarning("%s at %.2f s", pause ? "paused..." : "resuming", currentSeconds());
      paused_ = user_paused_ = pause;
    }
    stream_cv_.notify_one();
  }
}

void Replay::pauseStreamThread() {
  paused_ = true;
  if (stream_thread_.joinable() && stream_thread_id) {
    pthread_kill(stream_thread_id, SIGUSR1);  // Interrupt sleep in stream thread
  }
}

void Replay::onSegmentMerged() {
  if (stream_thread_.joinable()) {
    notifyEvent(onSegmentsMerged);
  }

  updateEvents([&]() {
    event_data_ = seg_mgr_.getEventData();
    // Wake up the stream thread if the current segment is loaded
    return seeking_to_ < 0 && event_data_->segments.count(current_segment_) > 0;
  });
  checkSeekProgress();

  // start stream thread
  if (!stream_thread_.joinable() && !event_data_->segments.empty()) {
    startStream();
  }
}

void Replay::startStream() {
  const auto &cur_segment = event_data_->segments.begin()->second;
  const auto &events = cur_segment->log->events;
  route_start_ts_ = events.front().mono_time;
  cur_mono_time_ += route_start_ts_ - 1;

  // get datetime from INIT_DATA, fallback to datetime in the route name
  route_date_time_ = route().datetime();
  auto it = std::find_if(events.cbegin(), events.cend(),
                         [](const Event &e) { return e.which == cereal::Event::Which::INIT_DATA; });
  if (it != events.cend()) {
    capnp::FlatArrayMessageReader reader(it->data);
    auto event = reader.getRoot<cereal::Event>();
    uint64_t wall_time = event.getInitData().getWallTimeNanos();
    if (wall_time > 0) {
      route_date_time_ = wall_time / 1e6;
    }
  }

  // write CarParams
  it = std::find_if(events.begin(), events.end(), [](const Event &e) { return e.which == cereal::Event::Which::CAR_PARAMS; });
  if (it != events.end()) {
    capnp::FlatArrayMessageReader reader(it->data);
    auto event = reader.getRoot<cereal::Event>();
    car_fingerprint_ = event.getCarParams().getCarFingerprint();
    capnp::MallocMessageBuilder builder;
    builder.setRoot(event.getCarParams());
    auto words = capnp::messageToFlatArray(builder);
    auto bytes = words.asBytes();
    Params().put("CarParams", (const char *)bytes.begin(), bytes.size());
    Params().put("CarParamsPersistent", (const char *)bytes.begin(), bytes.size());
  } else {
    rWarning("failed to read CarParams from current segment");
  }

  // start camera server
  if (!hasFlag(REPLAY_FLAG_NO_VIPC)) {
    std::pair<int, int> camera_size[MAX_CAMERAS] = {};
    for (auto type : ALL_CAMERAS) {
      if (auto &fr = cur_segment->frames[type]) {
        camera_size[type] = {fr->width, fr->height};
      }
    }
    camera_server_ = std::make_unique<CameraServer>(camera_size);
  }

  notifyEvent(onSegmentsMerged);

  timeline_.initialize(seg_mgr_.route_, route_start_ts_, !(flags_ & REPLAY_FLAG_NO_FILE_CACHE),
                       [this](std::shared_ptr<LogReader> log) {
                         if (onQLogLoaded) onQLogLoaded(log);
                       });
  // Start stream thread
  stream_thread_ = std::thread(&Replay::streamThread, this);
}

void Replay::publishMessage(const Event *e) {
  if (event_filter_ && event_filter_(e)) return;

  if (sm_ == nullptr) {
    auto bytes = e->data.asBytes();
    int ret = pm_->send(sockets_[e->which], (capnp::byte *)bytes.begin(), bytes.size());
    if (ret == -1) {
      rWarning("stop publishing %s due to multiple publishers error", sockets_[e->which]);
      sockets_[e->which] = nullptr;
    }
  } else {
    capnp::FlatArrayMessageReader reader(e->data);
    auto event = reader.getRoot<cereal::Event>();
    sm_->update_msgs(nanos_since_boot(), {{sockets_[e->which], event}});
  }
}

void Replay::publishFrame(SegmentManager::EventData *event_data, const Event *e) {
  CameraType cam;
  switch (e->which) {
    case cereal::Event::ROAD_ENCODE_IDX: cam = RoadCam; break;
    case cereal::Event::DRIVER_ENCODE_IDX: cam = DriverCam; break;
    case cereal::Event::WIDE_ROAD_ENCODE_IDX: cam = WideRoadCam; break;
    default: return;  // Invalid event type
  }

  if ((cam == DriverCam && !hasFlag(REPLAY_FLAG_DCAM)) || (cam == WideRoadCam && !hasFlag(REPLAY_FLAG_ECAM)))
    return;  // Camera isdisabled

  auto seg_it = event_data->segments.find(e->eidx_segnum);
  if ( seg_it != event_data->segments.end()) {
    if (auto &frame = seg_it->second->frames[cam]; frame) {
      camera_server_->pushFrame(cam, frame.get(), e);
    }
  }
}

void Replay::streamThread() {
  stream_thread_id = pthread_self();
  cereal::Event::Which cur_which = cereal::Event::Which::INIT_DATA;
  std::unique_lock lk(stream_lock_);

  while (true) {
    stream_cv_.wait(lk, [=]() { return exit_ || (events_ready_ && !paused_); });
    if (exit_) break;

    const auto local_events = event_data_;  // copy shared_ptr
    const auto &events_list = local_events->events;

    auto first = std::upper_bound(events_list.cbegin(), events_list.cend(),
                                  Event(cur_which, cur_mono_time_, {}));
    if (first == events_list.cend()) {
      rInfo("waiting for events...");
      events_ready_ = false;
      continue;
    }

    auto it = publishEvents(local_events.get(), first, events_list.cend());

    // Ensure frames are sent before unlocking to prevent race conditions
    if (camera_server_) {
      camera_server_->waitForSent();
    }

    if (it != events_list.cend()) {
      cur_which = it->which;
    } else if (!hasFlag(REPLAY_FLAG_NO_LOOP)) {
      // Check for loop end and restart if necessary
      int last_segment = seg_mgr_.route_.segments().rbegin()->first;
      if (current_segment_ >= last_segment && local_events->segments.count(last_segment)) {
        rInfo("reaches the end of route, restart from beginning");
        seeking_to_ = minSeconds();
        cur_mono_time_ = minSeconds() * 1e9 + route_start_ts_;
        current_segment_ = seeking_to_ / 60;
        seg_mgr_.setCurrentSegment(current_segment_);
      }
    }
  }
}

EventIterator Replay::publishEvents(SegmentManager::EventData *event_data, EventIterator first, EventIterator last) {
  uint64_t evt_start_ts = cur_mono_time_;
  uint64_t loop_start_ts = nanos_since_boot();
  double prev_replay_speed = speed_;

  for (; !paused_ && first != last; ++first) {
    const Event &evt = *first;
    int segment = toSeconds(evt.mono_time) / 60;

    if (current_segment_ != segment) {
      current_segment_ = segment;
      seg_mgr_.setCurrentSegment(current_segment_);
    }

    // Skip events if socket is not present
    if (!sockets_[evt.which]) continue;

    cur_mono_time_ = evt.mono_time;
    const uint64_t current_nanos = nanos_since_boot();
    const int64_t time_diff = (evt.mono_time - evt_start_ts) / speed_ - (current_nanos - loop_start_ts);

    // Reset timestamps for potential synchronization issues:
    // - A negative time_diff may indicate slow execution or system wake-up,
    // - A time_diff exceeding 1 second suggests a skipped segment.
    if ((time_diff < -1e9 || time_diff >= 1e9) || speed_ != prev_replay_speed) {
      evt_start_ts = evt.mono_time;
      loop_start_ts = current_nanos;
      prev_replay_speed = speed_;
    } else if (time_diff > 0) {
      precise_nano_sleep(time_diff, paused_);
    }

    if (paused_) break;

    if (evt.eidx_segnum == -1) {
      publishMessage(&evt);
    } else if (camera_server_) {
      if (speed_ > 1.0) {
        camera_server_->waitForSent();
      }
      publishFrame(event_data, &evt);
    }
  }

  return first;
}
