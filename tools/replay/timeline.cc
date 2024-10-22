#include "tools/replay/timeline.h"

#include <array>

#include "cereal/gen/cpp/log.capnp.h"

Timeline::Timeline() {
  timeline_ = std::make_shared<std::vector<Entry>>();
}

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

const std::shared_ptr<std::vector<Timeline::Entry>> Timeline::get() const {
  std::lock_guard lk(mutex_);
  return timeline_;
}

std::optional<uint64_t> Timeline::find(double cur_ts, FindFlag flag) {
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

void Timeline::assemble(const Route &route, uint64_t route_start_ts, bool local_cache,
                        std::function<void(std::shared_ptr<LogReader>)> callback) {
  for (const auto &segment : route.segments()) {
    if (exit_) break;  // Exit early if needed

    auto log = std::make_shared<LogReader>();
    if (!log->load(segment.second.qlog, &exit_, local_cache, 0, 3) || log->events.empty()) {
      continue;  // Skip if log loading fails or no events
    }

    auto entries = parseEvents(log->events, route_start_ts);
    callback(log);  // Notify listener

    std::scoped_lock lk(mutex_);
    timeline_->insert(timeline_->end(), entries.begin(), entries.end());
    std::sort(timeline_->begin(), timeline_->end(), [](const Entry &a, const Entry &b) {
      return a.start_time < b.start_time;
    });
  }
}

std::vector<Timeline::Entry> Timeline::parseEvents(const std::vector<Event> &events, uint64_t route_start_ts) {
  std::vector<Entry> entries;
  Entry *alert_entry = nullptr, *eng_entry = nullptr;
  auto alert_types = std::array{TimelineType::AlertInfo, TimelineType::AlertWarning, TimelineType::AlertCritical};

  for (const Event &e : events) {
    double seconds = (e.mono_time - route_start_ts) / 1e9;
    if (e.which == cereal::Event::Which::SELFDRIVE_STATE) {
      capnp::FlatArrayMessageReader reader(e.data);
      auto cs = reader.getRoot<cereal::Event>().getSelfdriveState();

      if (cs.getEnabled()) {
        if (eng_entry)
          eng_entry->end_time = seconds;
        else {
          eng_entry = &entries.emplace_back(Entry{seconds, seconds, TimelineType::Engaged});
        }
      } else {
        eng_entry = nullptr;
      }

      if (cs.getAlertSize() != cereal::SelfdriveState::AlertSize::NONE) {
        auto type = alert_types[(int)cs.getAlertStatus()];
        std::string text = cs.getAlertText1().cStr() + std::string("\n") + cs.getAlertText2().cStr();
        if (alert_entry && alert_entry->type == type && alert_entry->text == text) {
          alert_entry->end_time = seconds;
        } else {
          alert_entry = &entries.emplace_back(Entry{seconds, seconds, type, text});
        }
      } else {
        alert_entry = nullptr;
      }
    } else if (e.which == cereal::Event::Which::USER_FLAG) {
      entries.emplace_back(Entry{seconds, seconds, TimelineType::UserFlag});
    }
  }
  return entries;
}
