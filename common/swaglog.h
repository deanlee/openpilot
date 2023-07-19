#pragma once

#include <sstream>
#include "common/timing.h"

#define CLOUDLOG_DEBUG 10
#define CLOUDLOG_INFO 20
#define CLOUDLOG_WARNING 30
#define CLOUDLOG_ERROR 40
#define CLOUDLOG_CRITICAL 50

void cloudlog_e(int levelnum, const char* filename, int lineno, const char* func,
                const char* msg) /*__attribute__ ((format (printf, 6, 7)))*/;

void cloudlog_te(int levelnum, const char* filename, int lineno, const char* func,
                 const char* msg) /*__attribute__ ((format (printf, 6, 7)))*/;

void cloudlog_te(int levelnum, const char* filename, int lineno, const char* func,
                 uint32_t frame_id, const char* msg) /*__attribute__ ((format (printf, 6, 7)))*/;


class SwagLog {
public:
  SwagLog(int levelnum, const char *file, const char *func, int line) : levelnum(levelnum), file(file), func(func), line(line) {}
  ~SwagLog() { cloudlog_e(levelnum, file, line, func, os.str().c_str()); }

  SwagLog &init(const std::string &msg = {}) {
    if (!msg.empty()) os << msg;
    return *this;
  }

  SwagLog &init(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char* msg_buf = nullptr;
    int ret = vasprintf(&msg_buf, fmt, args);
    va_end(args);
    if (ret >= 0 && msg_buf) {
      os << msg_buf;
      free(msg_buf);
    }
    return *this;
  }

  template <typename U>
  SwagLog &operator<<(U arg) {
    if (os.tellp() > 0) os << " ";
    os << arg;
    return *this;
  }

private:
  const char *file, *func;
  int levelnum, line;
  std::ostringstream os;
};

#define LOG_FILE_LINE(levelnum) SwagLog(levelnum, __FILE__, __func__, __LINE__).init

#define cloudlog(lvl, msg) cloudlog_e(lvl, __FILE__, __LINE__, __func__, msg);

#define cloudlog_t(lvl, msg) cloudlog_te(lvl, __FILE__, __LINE__,  __func__, msg);


#define cloudlog_rl(burst, millis, lvl, fmt, ...)   \
{                                                   \
  static uint64_t __begin = 0;                      \
  static int __printed = 0;                         \
  static int __missed = 0;                          \
                                                    \
  int __burst = (burst);                            \
  int __millis = (millis);                          \
  uint64_t __ts = nanos_since_boot();               \
                                                    \
  if (!__begin) __begin = __ts;                     \
                                                    \
  if (__begin + __millis*1000000ULL < __ts) {       \
    if (__missed) {                                 \
      LOG_FILE_LINE(CLOUDLOG_WARNING)("cloudlog: %d messages suppressed", __missed); \
    }                                               \
    __begin = 0;                                    \
    __printed = 0;                                  \
    __missed = 0;                                   \
  }                                                 \
                                                    \
  if (__printed < __burst) {                        \
    LOG_FILE_LINE(lvl)(fmt, ## __VA_ARGS__);             \
    __printed++;                                    \
  } else {                                          \
    __missed++;                                     \
  }                                                 \
}

#define LOGT(...) LOG_FILE_LINE(CLOUDLOG_DEBUG)
#define LOGD LOG_FILE_LINE(CLOUDLOG_DEBUG)
#define LOG LOG_FILE_LINE(CLOUDLOG_INFO)
#define LOGW LOG_FILE_LINE(CLOUDLOG_WARNING)
#define LOGE LOG_FILE_LINE(CLOUDLOG_ERROR)

#define LOGD_100(fmt, ...) cloudlog_rl(2, 100, CLOUDLOG_DEBUG, fmt, ## __VA_ARGS__)
#define LOG_100(fmt, ...) cloudlog_rl(2, 100, CLOUDLOG_INFO, fmt, ## __VA_ARGS__)
#define LOGW_100(fmt, ...) cloudlog_rl(2, 100, CLOUDLOG_WARNING, fmt, ## __VA_ARGS__)
#define LOGE_100(fmt, ...) cloudlog_rl(2, 100, CLOUDLOG_ERROR, fmt, ## __VA_ARGS__)
