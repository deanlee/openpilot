#include "tools/replay/replay.h"

#include "cereal/services.h"
#include "common/params.h"
#include "tools/replay/util.h"

// Helper function to notify events with safety checks
template <typename Callback, typename... Args>
void notifyEvent(Callback &callback, Args &&...args) {
  if (callback) {
    callback(std::forward<Args>(args)...);
  }
}

Replay::Replay(const std::string &route, std::vector<std::string> allow, std::vector<std::string> block,
               SubMaster *sm, uint32_t flags, const std::string &data_dir)
    : flags_(flags), seg_mgr_(route, data_dir) {
  if (!(flags_ & REPLAY_FLAG_ALL_SERVICES)) {
    block.insert(block.end(), {"uiDebug", "userFlag"});
  }

  event_stream_.initialize(sm, flags, allow, block);
  // initializeSockets(allow, block);
  setupSegmentManager(allow);
}

Replay::~Replay() {
  stop();
}

void Replay::setupSegmentManager(const std::vector<std::string> &allow) {
  seg_mgr_.setFlags(flags_);
  seg_mgr_.setCallback([this]() { onSegmentMerged(); });

  // if (!allow.empty()) {
  //   std::vector<bool> filters(sockets_.size(), false);
  //   for (size_t i = 0; i < sockets_.size(); ++i) {
  //     filters[i] = (i == cereal::Event::Which::INIT_DATA || i == cereal::Event::Which::CAR_PARAMS || sockets_[i]);
  //   }
  //   seg_mgr_.setFilters(filters);
  // }
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


void Replay::seekTo(double seconds, bool relative) {
  // updateEvents([&]() {
  //   double target_time = relative ? seconds + currentSeconds() : seconds;
  //   target_time = std::max(double(0.0), target_time);
  //   int target_segment = (int)target_time / 60;
  //   if (!seg_mgr_.hasSegment(target_segment)) {
  //     rWarning("Invalid seek to %.2f s (segment %d)", target_time, target_segment);
  //     return true;
  //   }

  //   rInfo("Seeking to %.2f s (segment %d)", target_time, target_segment);
  //   current_segment_ = target_segment;
  //   cur_mono_time_ = route_start_ts_ + target_time * 1e9;
  //   seeking_to_ = target_time;
  //   return false;
  // });

  checkSeekProgress();
  // seg_mgr_.updateCurrentSegment(current_segment_);
}

void Replay::checkSeekProgress() {
  if (seeking_to_ < 0) return;

  if (isSegmentLoaded(seeking_to_ / 60)) {
    notifyEvent(onSeekedTo, seeking_to_);
    seeking_to_ = -1;
    // wake up stream thread
    // updateEvents([]() { return true; });
  } else {
    // Emit signal indicating the ongoing seek operation
    notifyEvent(onSeeking, seeking_to_);
  }
}

void Replay::seekToFlag(FindFlag flag) {
  if (auto next = timeline_.find(currentSeconds(), flag)) {
    seekTo(*next - 2, false);  // seek to 2 seconds before next
  }
}

void Replay::onSegmentMerged() {
  // if (stream_thread_.joinable()) {
    notifyEvent(onSegmentsMerged);
  // }

  event_stream_.setEvents(seg_mgr_.events());
  // updateEvents([&]() {
  //   events_ = seg_mgr_.events();
  //   // Wake up the stream thread if the current segment is loaded
  //   return seeking_to_ < 0 && events_->segments.count(current_segment_) > 0;
  // });
  checkSeekProgress();

  // start stream thread
  // if (!stream_thread_.joinable() && !events_->segments.empty()) {
    // startStream();
  // }
}

void Replay::startStream() {
  const auto &cur_segment = events_->segments.begin()->second;
  const auto &events = cur_segment->log->events;
  route_start_ts_ = events.front().mono_time;
  // cur_mono_time_ += route_start_ts_ - 1;

  // get datetime from INIT_DATA, fallback to datetime in the route name
  route_date_time_ = route().datetime();
  auto it = std::find_if(events.cbegin(), events.cend(),
                         [](const Event &e) { return e.which == cereal::Event::Which::INIT_DATA; });
  if (it != events.cend()) {
    capnp::FlatArrayMessageReader reader(it->data);
    auto event = reader.getRoot<cereal::Event>();
    uint64_t wall_time = event.getInitData().getWallTimeNanos();
    if (wall_time > 0) {
      route_date_time_ = QDateTime::fromMSecsSinceEpoch(wall_time / 1e6);
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

  notifyEvent(onSegmentsMerged);

  timeline_.initialize(seg_mgr_.route_, route_start_ts_, !(flags_ & REPLAY_FLAG_NO_FILE_CACHE),
                       [this](std::shared_ptr<LogReader> log) {
                         if (onQLogLoaded) onQLogLoaded(log);
                       });
  // Start stream thread
  event_stream_.start();
}

