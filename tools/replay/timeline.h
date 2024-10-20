#pragma once

#include <atomic>
#include <mutex>
#include <optional>
#include <thread>
#include <tuple>
#include <vector>

#include "tools/replay/route.h"

enum class TimelineType { None, Engaged, AlertInfo, AlertWarning, AlertCritical, UserFlag };
enum class FindFlag { nextEngagement, nextDisEngagement, nextUserFlag, nextInfo, nextWarning, nextCritical };

class Timeline {
public:
  Timeline() = default;
  ~Timeline();

  std::optional<uint64_t> find(double cur_ts, FindFlag flag);
  const std::vector<std::tuple<double, double, TimelineType>> get() const;
  void initialize(const Route &route, uint64_t route_start_ts, bool local_cache,
                  std::function<void(std::shared_ptr<LogReader>)> callback);

private:
  void assemble(const Route &route, uint64_t route_start_ts, bool local_cache,
                std::function<void(std::shared_ptr<LogReader>)> callback);

  mutable std::mutex timeline_lock;
  std::atomic<bool> exit_ = false;
  std::thread timeline_thread_;
  std::vector<std::tuple<double, double, TimelineType>> timeline_;
};
