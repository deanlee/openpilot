#include "tools/replay/timeline.h"

#include <array>
#include "cereal/gen/cpp/log.capnp.h"

Timeline::~Timeline() {
  exit_ = true;
  if (timeline_thread_.joinable()) {
    timeline_thread_.join();
  }
}

const std::vector<std::tuple<double, double, TimelineType>> Timeline::get() const {
  std::lock_guard lk(timeline_lock);
  return timeline_;
}

void Timeline::initialize(const Route &route, uint64_t route_start_ts, bool local_cache,
                          std::function<void(std::shared_ptr<LogReader>)> callback) {
  timeline_thread_ = std::thread(&Timeline::assemble, this, route, route_start_ts, local_cache, callback);
}

std::optional<uint64_t> Timeline::find(double cur_ts, FindFlag flag) {
  for (auto [start_ts, end_ts, type] : get()) {
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

void Timeline::assemble(const Route &route, uint64_t route_start_ts, bool local_cache,
                        std::function<void(std::shared_ptr<LogReader>)> callback) {
  std::vector<std::tuple<double, double, TimelineType>> entries;
  std::optional<size_t> engaged_idx, alert_idx;
  auto alert_types = std::array{TimelineType::AlertInfo, TimelineType::AlertWarning, TimelineType::AlertCritical};

  // Helper lambda to update the timeline
  auto update_timeline = [&](std::optional<size_t> &idx, double seconds, TimelineType type) {
    if (idx && std::get<2>(entries[*idx]) == type) {
      std::get<1>(entries[*idx]) = seconds;
    } else {
      idx = entries.size();
      entries.emplace_back(seconds, seconds, type);
    }
  };

  const auto &segments = route.segments();
  for (auto it = segments.cbegin(); it != segments.cend() && !exit_; ++it) {
    std::shared_ptr<LogReader> log = std::make_shared<LogReader>();
    if (!log->load(it->second.qlog, &exit_, local_cache, 0, 3) || log->events.empty()) continue;

    for (const Event &e : log->events) {
      double seconds = (e.mono_time - route_start_ts) / 1e9;
      if (e.which == cereal::Event::Which::SELFDRIVE_STATE) {
        capnp::FlatArrayMessageReader reader(e.data);
        auto cs = reader.getRoot<cereal::Event>().getSelfdriveState();

        if (cs.getEnabled()) {
          update_timeline(engaged_idx, seconds, TimelineType::Engaged);
        } else {
          engaged_idx.reset();
        }

        if (cs.getAlertSize() != cereal::SelfdriveState::AlertSize::NONE) {
          int idx = std::clamp((int)cs.getAlertStatus(), 0, (int)(alert_types.size() - 1));
          update_timeline(alert_idx, seconds, alert_types[idx]);
        } else {
          alert_idx.reset();
        }
      } else if (e.which == cereal::Event::Which::USER_FLAG) {
        entries.emplace_back(seconds, seconds, TimelineType::UserFlag);
      }
    }
    {
      std::scoped_lock lk(timeline_lock);
      timeline_ = entries;
      std::sort(timeline_.begin(), timeline_.end(), [](auto &l, auto &r) { return std::get<2>(l) < std::get<2>(r); });
    }
    callback(log);
  }
}
