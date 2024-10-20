#pragma once

#include <condition_variable>
#include <map>
#include <mutex>
#include <set>
#include <vector>

#include "tools/replay/logreader.h"
#include "tools/replay/route.h"

constexpr int MIN_SEGMENTS_CACHE = 5;

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

using SegmentMap = std::map<int, std::shared_ptr<Segment>>;

class SegmentManager {
public:
  struct EventData {
    std::shared_ptr<std::vector<Event>> events;  //  Events extracted from the segments
    SegmentMap segments;                         // Associated segments that contributed to these events
  };

  SegmentManager(const std::string &route_name, const std::string &data_dir = "");
  ~SegmentManager();

  bool load();
  void setCurrentSegment(int seg_num);
  void setCallback(const std::function<void()> &callback) { onSegmentMergedCallback_ = callback; }
  void setFilters(const std::vector<bool> &filters) { filters_ = filters; }
  void setFlags(uint32_t flags) { flags_ = flags; }
  const EventData getEventData() const { return event_data_; }
  bool hasSegment(int n) const {
    std::unique_lock lk(mutex_);
    return segments_.find(n) != segments_.end();
  }

  Route route_;
  int segment_cache_limit_ = MIN_SEGMENTS_CACHE;

private:
  void manageSegmentCache();
  void loadSegmentsInRange(SegmentMap::iterator begin, SegmentMap::iterator cur, SegmentMap::iterator end);
  void onSegmentLoadComplete(int seg_num, bool success);
  bool mergeSegments(const SegmentMap::iterator &begin, const SegmentMap::iterator &end);

  std::vector<bool> filters_;
  uint32_t flags_ = REPLAY_FLAG_NONE;

  mutable std::mutex mutex_;
  std::condition_variable cv_;
  std::thread thread_;
  std::atomic<int> cur_seg_num_ = -1;
  bool exit_ = false;

  SegmentMap segments_;
  EventData event_data_;
  std::function<void()> onSegmentMergedCallback_ = nullptr;
  std::set<int> merged_segments_;
};
