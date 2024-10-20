#pragma once
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <QFuture>
#include <vector>
#include <optional>

#include "tools/replay/route.h"

// one segment uses about 100M of memory
constexpr int MIN_SEGMENTS_CACHE = 5;

enum class FindFlag {
  nextEngagement,
  nextDisEngagement,
  nextUserFlag,
  nextInfo,
  nextWarning,
  nextCritical
};

enum REPLAY_FLAGS {
  REPLAY_FLAG_NONE = 0x0000,
  REPLAY_FLAG_DCAM = 0x0002,
  REPLAY_FLAG_ECAM = 0x0004,
  REPLAY_FLAG_NO_LOOP = 0x0010,
  REPLAY_FLAG_NO_FILE_CACHE = 0x0020,
  REPLAY_FLAG_QCAMERA = 0x0040,
  REPLAY_FLAG_NO_HW_DECODER = 0x0100,
  REPLAY_FLAG_NO_VIPC = 0x0400,
  REPLAY_FLAG_ALL_SERVICES = 0x0800,
};


enum class TimelineType { None, Engaged, AlertInfo, AlertWarning, AlertCritical, UserFlag };
typedef std::map<int, std::unique_ptr<Segment>> SegmentMap;

class SegmentManager {
 public:
  SegmentManager(const std::string &route, const std::string &data_dir = {});
  ~SegmentManager();
  bool load();
  inline double toSeconds(uint64_t mono_time) const { return (mono_time - route_start_ts_) / 1e9; }
  void updateSegmentsCache();
  void segmentLoadFinished(bool success);
  RouteLoadError lastRouteError() const { return route_.lastError(); }
  void loadSegmentInRange(SegmentMap::iterator begin, SegmentMap::iterator cur, SegmentMap::iterator end);
  void mergeSegments(const SegmentMap::iterator &begin, const SegmentMap::iterator &end);
  inline const std::map<int, std::unique_ptr<Segment>> &segments() const { return segments_; }
  inline QDateTime routeDateTime() const { return route_date_time_; }
  inline uint64_t routeStartNanos() const { return route_start_ts_; }
  inline double minSeconds() const { return !segments_.empty() ? segments_.begin()->first * 60 : 0; }
  inline double maxSeconds() const { return max_seconds_; }
  inline const std::string &carFingerprint() const { return car_fingerprint_; }
  inline int segmentCacheLimit() const { return segment_cache_limit; }
  inline void setSegmentCacheLimit(int n) { segment_cache_limit = std::max(MIN_SEGMENTS_CACHE, n); }
  inline const Route &route() const { return route_; }
  void buildTimeline();
  inline bool isSegmentMerged(int n) const { return merged_segments_.count(n) > 0; }
  inline const std::vector<std::tuple<double, double, TimelineType>> getTimeline() {
    std::lock_guard lk(timeline_lock);
    return timeline_;
  }

  SegmentMap segments_;
  QDateTime route_date_time_;
  std::atomic<double> max_seconds_ = 0;
  std::set<int> merged_segments_;
  Route route_;
  std::string car_fingerprint_;

  int segment_cache_limit = MIN_SEGMENTS_CACHE;
  std::mutex timeline_lock;
  QFuture<void> timeline_future;
  std::vector<std::tuple<double, double, TimelineType>> timeline_;
  std::optional<uint64_t> find(FindFlag flag);
  uint64_t route_start_ts_ = 0;
  std::atomic<int> current_segment_ = 0;
  std::vector<bool> filters_;
  std::atomic<uint32_t> flags_ = REPLAY_FLAG_NONE;

};
