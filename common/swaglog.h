#pragma once

#include <limits>
#include "common/timing.h"

#define CLOUDLOG_DEBUG 10
#define CLOUDLOG_INFO 20
#define CLOUDLOG_WARNING 30
#define CLOUDLOG_ERROR 40
#define CLOUDLOG_CRITICAL 50
const uint32_t SWAGLOG_NO_FRAME_ID = std::numeric_limits<uint32_t>::max();

#ifdef __GNUC__
#define SWAG_LOG_CHECK_FMT(a, b) __attribute__ ((format (printf, a, b)))
#else
#define SWAG_LOG_CHECK_FMT(a, b)
#endif

void cloudlog_e(int levelnum, const char* filename, int lineno, const char* func,
                bool log_timestamp, uint32_t frame_id, const char* fmt, ...) SWAG_LOG_CHECK_FMT(7, 8);

#define cloudlog(lvl, fmt, ...) cloudlog_e(lvl, __FILE__, __LINE__, __func__, \
                                           false, SWAGLOG_NO_FRAME_ID, fmt, ## __VA_ARGS__)

#define cloudlog_t(lvl, frame_id, fmt, ...) cloudlog_e(lvl, __FILE__, __LINE__, __func__, \
                                                       true, frame_id, fmt, ## __VA_ARGS__)


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
  if (!__begin) { __begin = __ts; }                 \
                                                    \
  if (__begin + __millis*1000000ULL < __ts) {       \
    if (__missed) {                                 \
      cloudlog(CLOUDLOG_WARNING, "cloudlog: %d messages suppressed", __missed); \
    }                                               \
    __begin = 0;                                    \
    __printed = 0;                                  \
    __missed = 0;                                   \
  }                                                 \
                                                    \
  if (__printed < __burst) {                        \
    cloudlog(lvl, fmt, ## __VA_ARGS__);             \
    __printed++;                                    \
  } else {                                          \
    __missed++;                                     \
  }                                                 \
}

#define LOGT_FRAME(frame_id, fmt, ...) cloudlog_t(CLOUDLOG_DEBUG, frame_id, fmt, ## __VA_ARGS__)
#define LOGT(fmt, ...) cloudlog_t(CLOUDLOG_DEBUG, SWAGLOG_NO_FRAME_ID, fmt, ## __VA_ARGS__)
#define LOGD(fmt, ...) cloudlog(CLOUDLOG_DEBUG, fmt, ## __VA_ARGS__)
#define LOG(fmt, ...) cloudlog(CLOUDLOG_INFO, fmt, ## __VA_ARGS__)
#define LOGW(fmt, ...) cloudlog(CLOUDLOG_WARNING, fmt, ## __VA_ARGS__)
#define LOGE(fmt, ...) cloudlog(CLOUDLOG_ERROR, fmt, ## __VA_ARGS__)

#define LOGD_100(fmt, ...) cloudlog_rl(2, 100, CLOUDLOG_DEBUG, fmt, ## __VA_ARGS__)
#define LOG_100(fmt, ...) cloudlog_rl(2, 100, CLOUDLOG_INFO, fmt, ## __VA_ARGS__)
#define LOGW_100(fmt, ...) cloudlog_rl(2, 100, CLOUDLOG_WARNING, fmt, ## __VA_ARGS__)
#define LOGE_100(fmt, ...) cloudlog_rl(2, 100, CLOUDLOG_ERROR, fmt, ## __VA_ARGS__)
