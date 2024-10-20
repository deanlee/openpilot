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
  struct Entry {
    double start_time;
    double end_time;
    TimelineType type;
  };

  Timeline() = default;
  ~Timeline();

  std::optional<uint64_t> find(double cur_ts, FindFlag flag);
  const std::vector<Timeline::Entry> get() const;
  void initialize(const Route &route, uint64_t route_start_ts, bool local_cache,
                  std::function<void(std::shared_ptr<LogReader>)> callback);

private:
  void assemble(const Route &route, uint64_t route_start_ts, bool local_cache,
                std::function<void(std::shared_ptr<LogReader>)> callback);
  void updateEntry(std::vector<Timeline::Entry> &entries, std::optional<size_t> &idx,
  double sec, TimelineType type);

  mutable std::mutex mutex_;
  std::thread thread_;
  std::atomic<bool> exit_ = false;
  std::vector<Timeline::Entry> timeline_;
};
