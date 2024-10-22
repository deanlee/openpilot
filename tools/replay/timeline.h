#pragma once

#include <atomic>
#include <optional>
#include <thread>
#include <vector>

#include "tools/replay/route.h"

enum class TimelineType { None, Engaged, AlertInfo, AlertWarning, AlertCritical, UserFlag };
enum class FindFlag { nextEngagement, nextDisEngagement, nextUserFlag, nextInfo, nextWarning, nextCritical };

class Timeline {
public:
  struct Entry {
    double start_time;
    double end_time;
    TimelineType type;
    std::string text1;
    std::string text2;
  };

  Timeline() : timeline_(std::make_shared<std::vector<Entry>>()) {}
  ~Timeline();

  void initialize(const Route &route, uint64_t route_start_ts, bool local_cache,
                  std::function<void(std::shared_ptr<LogReader>)> callback);
  std::optional<uint64_t> find(double cur_ts, FindFlag flag) const;
  std::optional<Entry> findAlert(double target_time) const;
  const std::shared_ptr<std::vector<Entry>> get() const { return timeline_; }

private:
  void assemble(const Route &route, uint64_t route_start_ts, bool local_cache,
                std::function<void(std::shared_ptr<LogReader>)> callback);
  void updateAlertEntry(const cereal::SelfdriveState::Reader &cs, std::optional<size_t> &idx, double seconds);
  void updateEngagedEntry(const cereal::SelfdriveState::Reader &cs, std::optional<size_t> &idx, double seconds);

  std::thread thread_;
  std::atomic<bool> exit_ = false;
  std::shared_ptr<std::vector<Entry>> timeline_;
  std::vector<Entry> entries_;
};
