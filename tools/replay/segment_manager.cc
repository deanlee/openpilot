#include "tools/replay/segment_manager.h"

SegmentManager::SegmentManager() {
}

bool SegmentManager::load() {
  if (!route_->load()) {
    rError("failed to load route %s from %s", route_->name().c_str(),
           route_->dir().empty() ? "server" : route_->dir().c_str());
    return false;
  }

  for (auto &[n, f] : route_->segments()) {
    bool has_log = !f.rlog.empty() || !f.qlog.empty();
    bool has_video = !f.road_cam.empty() || !f.qcamera.empty();
    if (has_log && (has_video || hasFlag(REPLAY_FLAG_NO_VIPC))) {
      segments_.insert({n, nullptr});
    }
  }
  if (segments_.empty()) {
    rInfo("no valid segments in route: %s", route_->name().c_str());
    return false;
  }
  rInfo("load route %s with %zu valid segments", route_->name().c_str(), segments_.size());
  max_seconds_ = (segments_.rbegin()->first + 1) * 60;
}

void SegmentManager::updateSegmentsCache() {
  auto cur = segments_.lower_bound(current_segment_.load());
  if (cur == segments_.end()) return;

  // Calculate the range of segments to load
  auto begin = std::prev(cur, std::min<int>(segment_cache_limit / 2, std::distance(segments_.begin(), cur)));
  auto end = std::next(begin, std::min<int>(segment_cache_limit, std::distance(begin, segments_.end())));
  begin = std::prev(end, std::min<int>(segment_cache_limit, std::distance(segments_.begin(), end)));

  loadSegmentInRange(begin, cur, end);
  mergeSegments(begin, end);

  // free segments out of current semgnt window.
  std::for_each(segments_.begin(), begin, [](auto &e) { e.second.reset(nullptr); });
  std::for_each(end, segments_.end(), [](auto &e) { e.second.reset(nullptr); });

  // start stream thread
  const auto &cur_segment = cur->second;
  if (stream_thread_ == nullptr && cur_segment->isLoaded()) {
    startStream(cur_segment.get());
  }
}

void SegmentManager::loadSegmentInRange(SegmentMap::iterator begin, SegmentMap::iterator cur, SegmentMap::iterator end) {
  auto loadNextSegment = [this](auto first, auto last) {
    auto it = std::find_if(first, last, [](const auto &seg_it) { return !seg_it.second || !seg_it.second->isLoaded(); });
    if (it != last && !it->second) {
      rDebug("loading segment %d...", it->first);
      it->second = std::make_unique<Segment>(it->first, route_->at(it->first), flags_, filters_);
      QObject::connect(it->second.get(), &Segment::loadFinished, this, &SegmentManager::segmentLoadFinished);
      return true;
    }
    return false;
  };

  // Try loading forward segments, then reverse segments
  if (!loadNextSegment(cur, end)) {
    loadNextSegment(std::make_reverse_iterator(cur), std::make_reverse_iterator(begin));
  }
}

void SegmentManager::mergeSegments(const SegmentMap::iterator &begin, const SegmentMap::iterator &end) {
  std::set<int> segments_to_merge;
  size_t new_events_size = 0;
  for (auto it = begin; it != end; ++it) {
    if (it->second && it->second->isLoaded()) {
      segments_to_merge.insert(it->first);
      new_events_size += it->second->log->events.size();
    }
  }

  if (segments_to_merge == merged_segments_) return;

  rDebug("merge segments %s", std::accumulate(segments_to_merge.begin(), segments_to_merge.end(), std::string{},
                                              [](auto &a, int b) { return a + (a.empty() ? "" : ", ") + std::to_string(b); })
                                  .c_str());

  std::vector<Event> new_events;
  new_events.reserve(new_events_size);

  // Merge events from segments_to_merge into new_events
  for (int n : segments_to_merge) {
    size_t size = new_events.size();
    const auto &events = segments_.at(n)->log->events;
    std::copy_if(events.begin(), events.end(), std::back_inserter(new_events),
                 [this](const Event &e) { return e.which < sockets_.size() && sockets_[e.which] != nullptr; });
    std::inplace_merge(new_events.begin(), new_events.begin() + size, new_events.end());
  }

  if (stream_thread_) {
    emit segmentsMerged();
  }

  updateEvents([&]() {
    events_.swap(new_events);
    merged_segments_ = segments_to_merge;
    // Wake up the stream thread if the current segment is loaded or invalid.
    return !seeking_to_ && (isSegmentMerged(current_segment_) || (segments_.count(current_segment_) == 0));
  });
  checkSeekProgress();
}

void SegmentManager::buildTimeline() {
  uint64_t engaged_begin = 0;
  bool engaged = false;

  auto alert_status = cereal::SelfdriveState::AlertStatus::NORMAL;
  auto alert_size = cereal::SelfdriveState::AlertSize::NONE;
  uint64_t alert_begin = 0;
  std::string alert_type;

  const TimelineType timeline_types[] = {
      [(int)cereal::SelfdriveState::AlertStatus::NORMAL] = TimelineType::AlertInfo,
      [(int)cereal::SelfdriveState::AlertStatus::USER_PROMPT] = TimelineType::AlertWarning,
      [(int)cereal::SelfdriveState::AlertStatus::CRITICAL] = TimelineType::AlertCritical,
  };

  const auto &route_segments = route_->segments();
  for (auto it = route_segments.cbegin(); it != route_segments.cend() && !exit_; ++it) {
    std::shared_ptr<LogReader> log(new LogReader());
    if (!log->load(it->second.qlog, &exit_, !hasFlag(REPLAY_FLAG_NO_FILE_CACHE), 0, 3) || log->events.empty()) continue;

    std::vector<std::tuple<double, double, TimelineType>> timeline;
    for (const Event &e : log->events) {
      if (e.which == cereal::Event::Which::SELFDRIVE_STATE) {
        capnp::FlatArrayMessageReader reader(e.data);
        auto event = reader.getRoot<cereal::Event>();
        auto cs = event.getSelfdriveState();

        if (engaged != cs.getEnabled()) {
          if (engaged) {
            timeline.push_back({toSeconds(engaged_begin), toSeconds(e.mono_time), TimelineType::Engaged});
          }
          engaged_begin = e.mono_time;
          engaged = cs.getEnabled();
        }

        if (alert_type != cs.getAlertType().cStr() || alert_status != cs.getAlertStatus()) {
          if (!alert_type.empty() && alert_size != cereal::SelfdriveState::AlertSize::NONE) {
            timeline.push_back({toSeconds(alert_begin), toSeconds(e.mono_time), timeline_types[(int)alert_status]});
          }
          alert_begin = e.mono_time;
          alert_type = cs.getAlertType().cStr();
          alert_size = cs.getAlertSize();
          alert_status = cs.getAlertStatus();
        }
      } else if (e.which == cereal::Event::Which::USER_FLAG) {
        timeline.push_back({toSeconds(e.mono_time), toSeconds(e.mono_time), TimelineType::UserFlag});
      }
    }

    if (it->first == route_segments.rbegin()->first) {
      if (engaged) {
        timeline.push_back({toSeconds(engaged_begin), toSeconds(log->events.back().mono_time), TimelineType::Engaged});
      }
      if (!alert_type.empty() && alert_size != cereal::SelfdriveState::AlertSize::NONE) {
        timeline.push_back({toSeconds(alert_begin), toSeconds(log->events.back().mono_time), timeline_types[(int)alert_status]});
      }

      max_seconds_ = std::ceil(toSeconds(log->events.back().mono_time));
      emit minMaxTimeChanged(route_segments.cbegin()->first * 60.0, max_seconds_);
    }
    {
      std::lock_guard lk(timeline_lock);
      timeline_.insert(timeline_.end(), timeline.begin(), timeline.end());
      std::sort(timeline_.begin(), timeline_.end(), [](auto &l, auto &r) { return std::get<2>(l) < std::get<2>(r); });
    }
    emit qLogLoaded(log);
  }
}

std::optional<uint64_t> SegmentManager::find(FindFlag flag) {
  int cur_ts = currentSeconds();
  for (auto [start_ts, end_ts, type] : getTimeline()) {
    if (type == TimelineType::Engaged) {
      if (flag == FindFlag::nextEngagement && start_ts > cur_ts) {
        return start_ts;
      } else if (flag == FindFlag::nextDisEngagement && end_ts > cur_ts) {
        return end_ts;
      }
    } else if (start_ts > cur_ts) {
      if ((flag == FindFlag::nextUserFlag && type == TimelineType::UserFlag) ||
          (flag == FindFlag::nextInfo && type == TimelineType::AlertInfo) ||
          (flag == FindFlag::nextWarning && type == TimelineType::AlertWarning) ||
          (flag == FindFlag::nextCritical && type == TimelineType::AlertCritical)) {
        return start_ts;
      }
    }
  }
  return std::nullopt;
}

void SegmentManager::segmentLoadFinished(bool success) {
  if (!success) {
    Segment *seg = qobject_cast<Segment *>(sender());
    rWarning("failed to load segment %d, removing it from current replay list", seg->seg_num);
    updateEvents([&]() {
      segments_.erase(seg->seg_num);
      return !segments_.empty();
    });
  }
  updateSegmentsCache();
}
