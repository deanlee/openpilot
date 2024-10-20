#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "tools/replay/camera.h"
#include "tools/replay/seg_mgr.h"
#include "tools/replay/timeline.h"

#define DEMO_ROUTE "a2a0ccea32023010|2023-07-27--13-01-19"

class Replay {
public:
  Replay(const std::string &route, std::vector<std::string> allow, std::vector<std::string> block, SubMaster *sm = nullptr,
         uint32_t flags = REPLAY_FLAG_NONE, const std::string &data_dir = "");
  ~Replay();
  bool load();
  RouteLoadError lastRouteError() const { return route().lastError(); }
  void start(int seconds = 0);
  void stop();
  void pause(bool pause);
  void seekToFlag(FindFlag flag);
  void seekTo(double seconds, bool relative);
  inline bool isPaused() const { return user_paused_; }
  inline int segmentCacheLimit() const { return seg_mgr_.segment_cache_limit_; }
  inline void setSegmentCacheLimit(int n) { seg_mgr_.segment_cache_limit_ = std::max(MIN_SEGMENTS_CACHE, n); }
  inline bool hasFlag(REPLAY_FLAGS flag) const { return flags_ & flag; }
  inline void addFlag(REPLAY_FLAGS flag) { flags_ |= flag; }
  inline void removeFlag(REPLAY_FLAGS flag) { flags_ &= ~flag; }
  const Route &route() const { return seg_mgr_.route_; }
  inline double currentSeconds() const { return double(cur_mono_time_ - route_start_ts_) / 1e9; }
  inline std::time_t routeDateTime() const { return route_date_time_; }
  inline uint64_t routeStartNanos() const { return route_start_ts_; }
  inline double toSeconds(uint64_t mono_time) const { return (mono_time - route_start_ts_) / 1e9; }
  inline double minSeconds() const { return min_seconds_; }
  inline double maxSeconds() const { return max_seconds_; }
  inline void setSpeed(float speed) { speed_ = speed; }
  inline float getSpeed() const { return speed_; }
  inline const std::string &carFingerprint() const { return car_fingerprint_; }
  bool isSegmentLoaded(int n) const {
    std::unique_lock lock(event_data_mutex_);
    return event_data_.segments.find(n) != event_data_.segments.end();
  }
  inline const std::shared_ptr<std::vector<Timeline::Entry>> getTimeline() const { return timeline_.get(); }
  inline const std::optional<Timeline::Entry> findAlertAtTime(double sec) const { return timeline_.findAlertAtTime(sec); }

  const SegmentManager::EventData getEventData() const { return event_data_; }
  void installEventFilter(std::function<bool(const Event *)> filter) { event_filter_ = filter; }

  // Event callback functions
  std::function<void()> onSegmentsMerged = nullptr;
  std::function<void(double)> onSeeking = nullptr;
  std::function<void(double)> onSeekedTo = nullptr;
  std::function<void(std::shared_ptr<LogReader>)> onQLogLoaded = nullptr;

private:
  void setupServices(const std::vector<std::string> &allow, const std::vector<std::string> &block);
  void setupSegmentManager(bool has_filters);
  void startStream();
  void streamThread();
  void handleSegmentMerge();
  void interruptStream(const std::function<bool()>& update_fn);
  std::vector<Event>::const_iterator publishEvents(std::vector<Event>::const_iterator first,
                                                   std::vector<Event>::const_iterator last);
  void publishMessage(const Event *e);
  void publishFrame(const Event *e);

  SegmentManager seg_mgr_;
  Timeline timeline_;

  pthread_t stream_thread_id = 0;
  std::thread stream_thread_;
  std::mutex stream_lock_;
  mutable std::mutex event_data_mutex_;
  bool user_paused_ = false;
  std::condition_variable stream_cv_;
  std::atomic<int> current_segment_ = 0;
  std::optional<double> seeking_to_;
  std::atomic<bool> exit_ = false;
  std::atomic<bool> interrupt_requested_ = false;
  bool events_ready_ = false;
  std::time_t route_date_time_;
  uint64_t route_start_ts_ = 0;
  std::atomic<uint64_t> cur_mono_time_ = 0;
  double min_seconds_ = 0;
  double max_seconds_ = 0;
  SubMaster *sm_ = nullptr;
  std::unique_ptr<PubMaster> pm_;
  std::vector<const char*> sockets_;
  std::unique_ptr<CameraServer> camera_server_;
  std::atomic<uint32_t> flags_ = REPLAY_FLAG_NONE;

  std::string car_fingerprint_;
  std::atomic<float> speed_ = 1.0;
  std::function<bool(const Event *)> event_filter_ = nullptr;

  SegmentManager::EventData event_data_;
};
