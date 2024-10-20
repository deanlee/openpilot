#include "tools/replay/seg_mgr.h"

SegmentManager::SegmentManager(const std::string &route, const std::string &data_dir)
    : route_(route, data_dir) {
}

SegmentManager::~SegmentManager() {
  {
    std::unique_lock lock(mutex_);
    exit_ = true;
    onSegmentLoaded_ = nullptr;
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

  for (const auto &[n, f] : route_.segments()) {
    if (!f.rlog.empty() || !f.qlog.empty()) {
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

void SegmentManager::updateCurrentSegment(int seg_num) {
  {
    std::unique_lock lock(mutex_);
    cur_seg_num_ = seg_num;
  }
  cv_.notify_one();
}

void SegmentManager::manageSegmentCache() {
  while (true) {
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
    mergeSegments(begin, end);

    // free segments out of current semgnt window.
    std::for_each(segments_.begin(), begin, [](auto &e) { e.second.reset(); });
    std::for_each(end, segments_.end(), [](auto &e) { e.second.reset(); });
  }
}

void SegmentManager::mergeSegments(const SegmentMap::iterator &begin, const SegmentMap::iterator &end) {
  std::set<int> segments_to_merge;
  size_t total_event_size = 0;
  for (auto it = begin; it != end; ++it) {
    if (it->second && it->second->isLoaded()) {
      segments_to_merge.insert(it->first);
      total_event_size += it->second->log->events.size();
    }
  }

  if (segments_to_merge == merged_segments_) return;

  rDebug("merging segments: %s", join(segments_to_merge, ", ").c_str());

  auto merged_events = std::make_shared<Events>();
  merged_events->events.reserve(total_event_size);

  // Merge segment events
  for (int n : segments_to_merge) {
    const auto &segment_events = segments_.at(n)->log->events;
    auto &merged_vector = merged_events->events;

    merged_events->segments[n] = segments_.at(n);
    size_t current_size = merged_vector.size();
    merged_vector.insert(merged_vector.end(), segment_events.begin(), segment_events.end());
    std::inplace_merge(merged_vector.begin(), merged_vector.begin() + current_size, merged_vector.end());
  }

  events_ = merged_events;

  if (onSegmentLoaded_) {
    onSegmentLoaded_();  // Notify listener that segments have been merged
  }
}

void SegmentManager::loadSegmentsInRange(SegmentMap::iterator begin, SegmentMap::iterator cur, SegmentMap::iterator end) {
  auto loadNextSegment = [this](auto first, auto last) {
    auto it = std::find_if(first, last, [](const auto &seg_it) { return !seg_it.second || !seg_it.second->isLoaded(); });
    if (it != last && !it->second) {
      rDebug("loading segment: %d...", it->first);
      it->second = std::make_shared<Segment>(
          it->first, route_.at(it->first), flags_, filters_,
          [this](int seg_num, bool success) {
            if (!success) {
              rWarning("failed to load segment %d, removing it from current replay list", seg_num);
            }
            updateCurrentSegment(cur_seg_num_);
          });
      return true;
    }
    return false;
  };

  // Try forward loading, then reverse if necessary
  if (!loadNextSegment(cur, end)) {
    loadNextSegment(std::make_reverse_iterator(cur), std::make_reverse_iterator(begin));
  }
}
