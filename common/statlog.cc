#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "common/statlog.h"
#include "common/util.h"

#include <cstdarg>
#include <stdio.h>
#include <mutex>
#include <zmq.h>

class StatlogState : public LogState {
  public:
    StatlogState() : LogState("ipc:///tmp/stats") {}
};

static StatlogState s = {};

static void log(const char* metric_type, const char* metric, const char* fmt, ...) {
  std::lock_guard lk(s.lock);
  if (!s.initialized) s.initialize();

  va_list args;
  va_start(args, fmt);
  std::string value = util::string_format_v(fmt, args);
  va_end(args);

  if (!value.empty()) {
    std::string line = util::string_format("%s:%s|%s", metric, value.c_str(), metric_type);
    zmq_send(s.sock, line.data(), line.size(), ZMQ_NOBLOCK);
  }
}

void statlog_log(const char* metric_type, const char* metric, int value) {
  log(metric_type, metric, "%d", value);
}

void statlog_log(const char* metric_type, const char* metric, float value) {
  log(metric_type, metric, "%f", value);
}
