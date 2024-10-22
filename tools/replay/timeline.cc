#include "tools/replay/timeline.h"

#include <array>

#include "cereal/gen/cpp/log.capnp.h"

Timeline::~Timeline() {
  exit_.store(true);
  if (thread_.joinable()) {
    thread_.join();
  }
}

void Timeline::initialize(const Route &route, uint64_t route_start_ts, bool local_cache,
                          std::function<void(std::shared_ptr<LogReader>)> callback) {
  thread_ = std::thread(&Timeline::assemble, this, route, route_start_ts, local_cache, callback);
}

std::optional<uint64_t> Timeline::find(double cur_ts, FindFlag flag) const {
  for (const auto &entry : *get()) {
    if (entry.type == TimelineType::Engaged) {
      if (flag == FindFlag::nextEngagement && entry.start_time > cur_ts) {
        return entry.start_time;
      } else if (flag == FindFlag::nextDisEngagement && entry.end_time > cur_ts) {
        return entry.end_time;
      }
    } else if (entry.start_time > cur_ts) {
      if ((flag == FindFlag::nextUserFlag && entry.type == TimelineType::UserFlag) ||
          (flag == FindFlag::nextInfo && entry.type == TimelineType::AlertInfo) ||
          (flag == FindFlag::nextWarning && entry.type == TimelineType::AlertWarning) ||
          (flag == FindFlag::nextCritical && entry.type == TimelineType::AlertCritical)) {
        return entry.start_time;
      }
    }
  }
  return std::nullopt;
}

std::optional<Timeline::Entry> Timeline::findAlert(double target_time) const {
  for (const auto &entry : *get()) {
    if (entry.start_time > target_time) break;
    if (entry.end_time >= target_time && entry.type >= TimelineType::AlertInfo) {
      return entry;
    }
  }
  return std::nullopt;
}

void Timeline::assemble(const Route &route, uint64_t route_start_ts, bool local_cache,
                        std::function<void(std::shared_ptr<LogReader>)> callback) {
  std::optional<size_t> eng_idx, alert_idx;

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
        updateEngagedEntry(cs, eng_idx, seconds);
        updateAlertEntry(cs, alert_idx, seconds);
      } else if (e.which == cereal::Event::Which::USER_FLAG) {
        entries_.emplace_back(Entry{seconds, seconds, TimelineType::UserFlag});
      }
    }

    callback(log);  // Notify listener

     // Sort entries and atomically update timeline
    std::sort(entries_.begin(), entries_.end(), [](auto &a, auto &b) { return a.start_time < b.start_time; });
    timeline_ = std::make_shared<std::vector<Entry>>(entries_);
  }
}

void Timeline::updateEngagedEntry(const cereal::SelfdriveState::Reader &cs, std::optional<size_t> &idx, double seconds) {
  if (idx) entries_[*idx].end_time = seconds;
  if (cs.getEnabled()) {
    if (!idx) {
      idx = entries_.size();
      entries_.emplace_back(Entry{seconds, seconds, TimelineType::Engaged});
    }
  } else {
    idx.reset();
  }
}

void Timeline::updateAlertEntry(const cereal::SelfdriveState::Reader &cs, std::optional<size_t> &idx, double seconds) {
  static auto alert_types = std::array{TimelineType::AlertInfo, TimelineType::AlertWarning, TimelineType::AlertCritical};

  Entry *entry = idx ? &entries_[*idx] : nullptr;
  if (entry) entry->end_time = seconds;
  if (cs.getAlertSize() != cereal::SelfdriveState::AlertSize::NONE) {
    auto type = alert_types[(int)cs.getAlertStatus()];
    std::string text1 = cs.getAlertText1().cStr();
    std::string text2 = cs.getAlertText2().cStr();
    if (!entry || entry->type != type || entry->text1 != text1 || entry->text2 != text2) {
      idx = entries_.size();
      entries_.emplace_back(Entry{seconds, seconds, type, text1, text2});  // Start a new entry
    }
  } else {
    idx.reset();
  }
}
