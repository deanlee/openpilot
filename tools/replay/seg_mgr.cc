#include "tools/replay/seg_mgr.h"

SegmentManager::SegmentManager(const std::string &route_name, const std::string &data_dir)
    : route_(route_name, data_dir) {
}

SegmentManager::~SegmentManager() {
  {
    std::unique_lock lock(mutex_);
    exit_ = true;
    onSegmentMergedCallback_ = nullptr;
  }
  cv_.notify_one();
  if (thread_.joinable()) {
    thread_.join();
  }
}

bool SegmentManager::load() {
  if (!route_.load()) {
    rError("failed to load route: %s", route_.name().c_str());
    return false;
  }

  for (const auto &[n, file] : route_.segments()) {
    if (!file.rlog.empty() || !file.qlog.empty()) {
      segments_.insert({n, nullptr});
    }
  }

  if (segments_.empty()) {
    rInfo("no valid segments in route: %s", route_.name().c_str());
    return false;
  }

  rInfo("load route %s with %zu valid segments", route_.name().c_str(), segments_.size());
  thread_ = std::thread(&SegmentManager::manageSegmentCache, this);
  return true;
}

void SegmentManager::setCurrentSegment(int seg_num) {
  {
    std::unique_lock lock(mutex_);
    cur_seg_num_ = seg_num;
  }
  cv_.notify_one();
}

void SegmentManager::manageSegmentCache() {
  while (!exit_) {
    std::unique_lock lock(mutex_);
    cv_.wait(lock);
    if (exit_) break;

    auto cur = segments_.lower_bound(cur_seg_num_);
    if (cur == segments_.end()) continue;

    // Calculate the range of segments to load
    auto begin = std::prev(cur, std::min<int>(segment_cache_limit_ / 2, std::distance(segments_.begin(), cur)));
    auto end = std::next(begin, std::min<int>(segment_cache_limit_, std::distance(begin, segments_.end())));
    begin = std::prev(end, std::min<int>(segment_cache_limit_, std::distance(segments_.begin(), end)));

    loadSegmentsInRange(begin, cur, end);
    bool merged = mergeSegments(begin, end);

    lock.unlock();

    if (merged && onSegmentMergedCallback_) {
      onSegmentMergedCallback_();  // Notify listener that segments have been merged
    }

    // Free segments outside of the current segment window
    std::for_each(segments_.begin(), begin, [](auto &segment) { segment.second.reset(); });
    std::for_each(end, segments_.end(), [](auto &segment) { segment.second.reset(); });
  }
}

bool SegmentManager::mergeSegments(const SegmentMap::iterator &begin, const SegmentMap::iterator &end) {
  std::set<int> segments_to_merge;
  size_t total_event_count = 0;
  for (auto it = begin; it != end; ++it) {
    if (it->second && it->second->isLoaded()) {
      segments_to_merge.insert(it->first);
      total_event_count += it->second->log->events.size();
    }
  }

  if (segments_to_merge == merged_segments_) return false;

  rDebug("merging segments: %s", join(segments_to_merge, ", ").c_str());

  EventData merged_event_data;
  merged_event_data.events = std::make_unique<std::vector<Event>>();
  merged_event_data.events->reserve(total_event_count);

  // Merge segment events
  for (int n : segments_to_merge) {
    const auto &events = segments_.at(n)->log->events;
    auto &merged_events = merged_event_data.events;
    size_t previous_size = merged_events->size();

    merged_events->insert(merged_events->end(), events.begin(), events.end());
    std::inplace_merge(merged_events->begin(), merged_events->begin() + previous_size, merged_events->end());
    merged_event_data.segments[n] = segments_.at(n);
  }

  event_data_ = merged_event_data;
  merged_segments_ = segments_to_merge;

  return true;
}

void SegmentManager::loadSegmentsInRange(SegmentMap::iterator begin, SegmentMap::iterator cur, SegmentMap::iterator end) {
  auto tryLoadSegment = [this](auto first, auto last) {
    auto it = std::find_if(first, last, [](const auto &seg_it) {
      return !seg_it.second || !seg_it.second->isLoaded();
    });

    if (it != last && !it->second) {
      rDebug("loading segment: %d...", it->first);
      it->second = std::make_shared<Segment>(
          it->first, route_.at(it->first), flags_, filters_,
          [this](int seg_num, bool success) { onSegmentLoadComplete(seg_num, success); });
      return true;
    }
    return false;
  };

  // Try forward loading, then reverse if necessary
  if (!tryLoadSegment(cur, end)) {
    tryLoadSegment(std::make_reverse_iterator(cur), std::make_reverse_iterator(begin));
  }
}

void SegmentManager::onSegmentLoadComplete(int seg_num, bool success) {
  if (!success) {
    rWarning("failed to load segment %d, removing it from current replay list", seg_num);
    std::unique_lock lock(mutex_);
    segments_.erase(seg_num);
  }
  setCurrentSegment(cur_seg_num_);
}
