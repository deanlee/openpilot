#pragma once

#include <stdio.h>
#include <stdint.h>
#include <pthread.h>
#include <memory>
#include <mutex>
#include <bzlib.h>
#include <kj/array.h>
#include <capnp/serialize.h>
#include "common/util.h"

#if defined(QCOM) || defined(QCOM2)
const std::string LOG_ROOT = "/data/media/0/realdata";
#else
const std::string LOG_ROOT = util::getenv_default("HOME", "/.comma/media/0/realdata", "/data/media/0/realdata");
#endif

#define LOGGER_MAX_HANDLES 16

class BZFile {
 public:
  BZFile(const char* path) {
    file = fopen(path, "wb");
    assert(file != nullptr);
    int bzerror;
    bz_file = BZ2_bzWriteOpen(&bzerror, file, 9, 0, 30);
    assert(bzerror == BZ_OK);
  }
  ~BZFile() {
    int bzerror;
    BZ2_bzWriteClose(&bzerror, bz_file, 0, nullptr, nullptr);
    if (bzerror != BZ_OK) {
      LOGE("BZ2_bzWriteClose error, bzerror=%d", bzerror);
    }
    int err = fclose(file);
    assert(err == 0);
  }
  inline void write(void* data, size_t size) {
    int bzerror;
    BZ2_bzWrite(&bzerror, bz_file, data, size);
    if (bzerror != BZ_OK && !error_logged) {
      LOGE("BZ2_bzWrite error, bzerror=%d", bzerror);
      error_logged = true;
    }
  }
  inline void write(kj::ArrayPtr<capnp::byte> array) { write(array.begin(), array.size()); }

 private:
  bool error_logged = false;
  FILE* file = nullptr;
  BZFILE* bz_file = nullptr;
};

class LoggerHandle {
public:
  LoggerHandle(const std::string &segment_path, const std::string &log_name, bool has_qlog);
  ~LoggerHandle();
  void write(uint8_t* data, size_t data_size, bool in_qlog);
  inline void write(kj::ArrayPtr<capnp::byte> array, bool in_qlog) { write(array.begin(), array.size(), in_qlog); }

 private:
  std::mutex lock;
  std::string lock_path;
  std::unique_ptr<BZFile> log, q_log;
};

class LoggerState {
public:
  LoggerState(const std::string &log_root, const std::string& log_name, bool has_qlog);
  ~LoggerState();
  inline std::shared_ptr<LoggerHandle> get_handle() { return cur_handle; };
  inline void write(uint8_t* data, size_t data_size, bool in_qlog) {
    if (cur_handle) cur_handle->write(data, data_size, in_qlog);
  }
  inline void write(kj::ArrayPtr<capnp::byte> array, bool in_qlog) { write(array.begin(), array.size(), in_qlog); }
  std::string next(int* out_part);
  int part;

private:
  std::string segment_path, route_path, log_name;
  bool has_qlog;
  kj::Array<capnp::word> init_data;
  std::shared_ptr<LoggerHandle> cur_handle;
};

int logger_mkpath(char* file_path);
kj::Array<capnp::word> logger_build_boot();
kj::Array<capnp::word> logger_build_init_data();

