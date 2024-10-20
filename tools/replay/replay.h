#pragma once

#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <shared_mutex>
#include <string>
#include <utility>
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
  RouteLoadError lastRouteError() const { return seg_mgr_.route_.lastError(); }
  void start(int seconds = 0);
  void stop();
  void pause(bool pause);
  void seekToFlag(FindFlag flag);
  void seekTo(double seconds, bool relative);
  inline bool isPaused() const { return user_paused_; }
  // the filter is called in streaming thread.try to return quickly from it to avoid blocking streaming.
  // the filter function must return true if the event should be filtered.
  // otherwise it must return false.
  void installEventFilter(std::function<bool(const Event *)> filter) { event_filter_ = filter; }

  inline int segmentCacheLimit() const { return seg_mgr_.segment_cache_limit_; }
  inline void setSegmentCacheLimit(int n) { seg_mgr_.segment_cache_limit_ = std::max(MIN_SEGMENTS_CACHE, n); }
  inline bool hasFlag(REPLAY_FLAGS flag) const { return flags_ & flag; }
  inline void addFlag(REPLAY_FLAGS flag) { flags_ |= flag; }
  inline void removeFlag(REPLAY_FLAGS flag) { flags_ &= ~flag; }
  inline const Route& route() const { return seg_mgr_.route_; }
  inline double currentSeconds() const { return double(cur_mono_time_ - route_start_ts_) / 1e9; }
  inline QDateTime routeDateTime() const { return route_date_time_; }
  inline uint64_t routeStartNanos() const { return route_start_ts_; }
  inline double toSeconds(uint64_t mono_time) const { return (mono_time - route_start_ts_) / 1e9; }
  inline double minSeconds() const { return min_seconds_; }
  inline double maxSeconds() const { return max_seconds_; }
  inline void setSpeed(float speed) { speed_ = speed; }
  inline float getSpeed() const { return speed_; }
  inline const std::string &carFingerprint() const { return car_fingerprint_; }
  inline bool isSegmentLoaded(int n) const { return events_ && events_->segments.count(n); }
  inline const Timeline &getTimeline() const { return timeline_; }
  inline const std::shared_ptr<SegmentManager::Events> events() const {
    std::shared_lock<std::shared_mutex> lock(events_mutex_);
    return events_;
  }

  // Event callback functions
  std::function<void()> onSegmentsMerged = nullptr;
  std::function<void(double)> onSeeking = nullptr;
  std::function<void(double)> onSeekedTo = nullptr;
  std::function<void(std::shared_ptr<LogReader>)> onQLogLoaded = nullptr;

private:
  void initializeSockets(std::vector<std::string> allow, std::vector<std::string> block);
  void setupSegmentManager(const std::vector<std::string> &allow);
  void pauseStreamThread();
  void startStream();
  void streamThread();
  void onSegmentMerged();
  void publishMessage(const Event *e);
  void publishFrame(const Event *e);
  void checkSeekProgress();
  void updateEvents(const std::function<bool()>& update_events_function);
  std::vector<Event>::const_iterator publishEvents(std::vector<Event>::const_iterator first,
                                                   std::vector<Event>::const_iterator last);

  SegmentManager seg_mgr_;
  Timeline timeline_;

  pthread_t stream_thread_id = 0;
  std::thread stream_thread_;
  std::mutex stream_lock_;
  bool user_paused_ = false;
  std::condition_variable stream_cv_;
  std::atomic<int> current_segment_ = 0;
  std::atomic<double> seeking_to_ = -1;
  std::atomic<bool> exit_ = false;
  std::atomic<bool> paused_ = false;
  bool events_ready_ = false;
  QDateTime route_date_time_;
  uint64_t route_start_ts_ = 0;
  std::atomic<uint64_t> cur_mono_time_ = 0;
  double min_seconds_ = 0;
  std::atomic<double> max_seconds_ = 0;

  SubMaster *sm_ = nullptr;
  std::unique_ptr<PubMaster> pm_;
  std::vector<const char*> sockets_;
  std::unique_ptr<CameraServer> camera_server_;
  std::atomic<uint32_t> flags_ = REPLAY_FLAG_NONE;

  std::string car_fingerprint_;
  std::atomic<float> speed_ = 1.0;
  std::function<bool(const Event *)> event_filter_ = nullptr;

  mutable std::shared_mutex events_mutex_;
  std::shared_ptr<SegmentManager::Events> events_;
};
