#include "tools/replay/timeline.h"

#include <array>

#include "cereal/gen/cpp/log.capnp.h"

Timeline::~Timeline() {
  exit_ = true;
  if (thread_.joinable()) {
    thread_.join();
  }
}

void Timeline::initialize(const Route &route, uint64_t route_start_ts, bool local_cache,
                          std::function<void(std::shared_ptr<LogReader>)> callback) {
  thread_ = std::thread(&Timeline::assemble, this, route, route_start_ts, local_cache, callback);
}

const std::vector<Timeline::Entry> Timeline::get() const {
  std::lock_guard lk(mutex_);
  return timeline_;
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
  std::vector<Timeline::Entry> entries;
  std::optional<size_t> eng_idx, alert_idx;
  auto alert_types = std::array{TimelineType::AlertInfo, TimelineType::AlertWarning, TimelineType::AlertCritical};

  for (const auto &segment : route.segments()) {
    if (exit_) break;  // Exit early if needed

    auto log = std::make_shared<LogReader>();
    if (!log->load(segment.second.qlog, &exit_, local_cache, 0, 3) || log->events.empty()) {
      continue;  // Skip if log loading fails or no events
    }

    for (const Event &e : log->events) {
      double seconds = (e.mono_time - route_start_ts) / 1e9;
      if (e.which == cereal::Event::Which::SELFDRIVE_STATE) {
        capnp::FlatArrayMessageReader reader(e.data);
        auto cs = reader.getRoot<cereal::Event>().getSelfdriveState();

        if (cs.getEnabled()) {
          updateEntry(entries, eng_idx, seconds, TimelineType::Engaged);
        } else {
          eng_idx.reset();
        }

        if (cs.getAlertSize() != cereal::SelfdriveState::AlertSize::NONE) {
          int idx = std::clamp((int)cs.getAlertStatus(), 0, (int)(alert_types.size() - 1));
          updateEntry(entries, alert_idx, seconds, alert_types[idx]);
        } else {
          alert_idx.reset();
        }
      } else if (e.which == cereal::Event::Which::USER_FLAG) {
        entries.emplace_back(Timeline::Entry{seconds, seconds, TimelineType::UserFlag});
      }
    }

    callback(log);  // Notify listener

    std::scoped_lock lk(mutex_);
    timeline_ = entries;
    std::sort(timeline_.begin(), timeline_.end(), [](auto &a, auto &b) { return a.type < b.type; });
  }
}

void Timeline::updateEntry(std::vector<Timeline::Entry> &entries, std::optional<size_t> &idx, double sec, TimelineType type) {
  if (idx && entries[*idx].type == type) {
    entries[*idx].end_time = sec;  // Update end time for ongoing entry
  } else {
    idx = entries.size();
    entries.emplace_back(Timeline::Entry{sec, sec, type});  // Start a new entry
  }
}
