#include "tools/replay/stream.h"

#include <capnp/dynamic.h>

#include <csignal>

#include "cereal/services.h"

static void interrupt_sleep_handler(int signal) {}

EventStream::EventStream() {
  std::signal(SIGUSR1, interrupt_sleep_handler);  // Register signal handler for SIGUSR1
}

void EventStream::initialize(SubMaster *sm, uint32_t flags, std::vector<std::string> allow, std::vector<std::string> block) {
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
  if (sm_ == nullptr) {
    pm_ = std::make_unique<PubMaster>(active_services);
  }
}

void EventStream::setEvents(std::shared_ptr<SegmentManager::Events> events) {
  updateEvents([&]() {
    events_ = events;
    return true;
  });

  if (!stream_thread_.joinable() && events_->segments.size() > 0) {
    start();
  }
}

void EventStream::start() {
  // start camera server
  const auto &segment = events_->segments.begin()->second;
  if (!(flags_ & REPLAY_FLAG_NO_VIPC)) {
    std::pair<int, int> camera_size[MAX_CAMERAS] = {};
    for (auto type : ALL_CAMERAS) {
      if (auto &fr = segment->frames[type]) {
        camera_size[type] = {fr->width, fr->height};
      }
    }
    camera_server_ = std::make_unique<CameraServer>(camera_size);
  }
  stream_thread_ = std::thread(&EventStream::streamThread, this);
}

void EventStream::pause(bool pause) {
  if (user_paused_ != pause) {
    pauseStreamThread();
    {
      std::unique_lock lock(stream_lock_);
      // rWarning("%s at %.2f s", pause ? "paused..." : "resuming", currentSeconds());
      paused_ = user_paused_ = pause;
    }
    stream_cv_.notify_one();
  }
}

void EventStream::stop() {
  exit_ = true;
  if (stream_thread_.joinable()) {
    rInfo("shutdown: in progress...");
    pauseStreamThread();
    stream_cv_.notify_one();
    stream_thread_.join();
    rInfo("shutdown: done");
  }

  camera_server_.reset(nullptr);
}

void EventStream::updateEvents(const std::function<bool()> &update_events_function) {
  pauseStreamThread();
  {
    std::unique_lock lk(stream_lock_);
    events_ready_ = update_events_function();
    paused_ = user_paused_;
  }
  stream_cv_.notify_one();
}

void EventStream::pauseStreamThread() {
  paused_ = true;
  // Send SIGUSR1 to interrupt clock_nanosleep
  if (stream_thread_.joinable() && stream_thread_id) {
    pthread_kill(stream_thread_id, SIGUSR1);
  }
}

void EventStream::publishMessage(const Event *e) {
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

void EventStream::publishFrame(const Event *e) {
  // CameraType cam;
  // switch (e->which) {
  //   case cereal::Event::ROAD_ENCODE_IDX: cam = RoadCam; break;
  //   case cereal::Event::DRIVER_ENCODE_IDX: cam = DriverCam; break;
  //   case cereal::Event::WIDE_ROAD_ENCODE_IDX: cam = WideRoadCam; break;
  //   default: return;  // Invalid event type
  // }

  // if ((cam == DriverCam && !(flags_ & REPLAY_FLAG_DCAM)) || (cam == WideRoadCam && !(flags_ & REPLAY_FLAG_ECAM)))
  //   return;  // Camera isdisabled

  // if (auto seg_it = events_->segments.find(e->eidx_segnum); seg_it != events_->segments.end()) {
  //   if (auto &frame = seg_it->second->frames[cam]; frame) {
  //     camera_server_->pushFrame(cam, frame.get(), e);
  //   }
  // }
}

void EventStream::streamThread() {
  stream_thread_id = pthread_self();
  cereal::Event::Which cur_which = cereal::Event::Which::INIT_DATA;
  std::unique_lock lk(stream_lock_);

  while (true) {
    stream_cv_.wait(lk, [=]() { return exit_ || (events_ready_ && !paused_); });
    if (exit_) break;

    Event event(cur_which, cur_mono_time_, {});
    const auto local_events = events_;
    const auto &events_list = local_events->events;
    auto first = std::upper_bound(events_list.cbegin(), events_list.cend(), event);
    if (first == events_list.cend()) {
      rInfo("waiting for events...");
      events_ready_ = false;
      continue;
    }

    auto it = publishEvents(first, events_list.cend());

    // Ensure frames are sent before unlocking to prevent race conditions
    if (camera_server_) {
      camera_server_->waitForSent();
    }

    if (it != events_list.cend()) {
      cur_which = it->which;
    } else if (!(flags_ & REPLAY_FLAG_NO_LOOP)) {
      // Check for loop end and restart if necessary
      // int last_segment = seg_mgr_.route_.segments().rbegin()->first;
      // if (current_segment_ >= last_segment && local_events->segments.count(last_segment)) {
      //   rInfo("reaches the end of route, restart from beginning");
      //   seeking_to_ = minSeconds();
      //   cur_mono_time_ = minSeconds() * 1e9 + route_start_ts_;
      //   current_segment_ = seeking_to_ / 60;
      //   seg_mgr_.updateCurrentSegment(current_segment_);
      // }
    }
  }
}

std::vector<Event>::const_iterator EventStream::publishEvents(std::vector<Event>::const_iterator first,
                                                              std::vector<Event>::const_iterator last) {
  uint64_t evt_start_ts = cur_mono_time_;
  uint64_t loop_start_ts = nanos_since_boot();
  double prev_replay_speed = speed_;

  for (; !paused_ && first != last; ++first) {
    const Event &evt = *first;
    int segment = 0;  // toSeconds(evt.mono_time) / 60;

    if (current_segment_ != segment) {
      current_segment_ = segment;
      // seg_mgr_.updateCurrentSegment(current_segment_);
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
      publishFrame(&evt);
    }
  }

  return first;
}
