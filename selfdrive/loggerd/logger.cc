#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>

#include <iostream>
#include <fstream>
#include <streambuf>
#ifdef QCOM
#include <cutils/properties.h>
#endif

#include "common/swaglog.h"
#include "common/params.h"
#include "common/version.h"
#include "messaging.hpp"
#include "logger.h"


// ***** logging helpers *****

void append_property(const char* key, const char* value, void *cookie) {
  std::vector<std::pair<std::string, std::string> > *properties =
    (std::vector<std::pair<std::string, std::string> > *)cookie;

  properties->push_back(std::make_pair(std::string(key), std::string(value)));
}

int logger_mkpath(char* file_path) {
  assert(file_path && *file_path);
  char* p;
  for (p=strchr(file_path+1, '/'); p; p=strchr(p+1, '/')) {
    *p = '\0';
    if (mkdir(file_path, 0777)==-1) {
      if (errno != EEXIST) {
        *p = '/';
        return -1;
      }
    }
    *p = '/';
  }
  return 0;
}

// ***** log metadata *****
kj::Array<capnp::word> logger_build_boot() {
  MessageBuilder msg;
  auto boot = msg.initEvent().initBoot();

  boot.setWallTimeNanos(nanos_since_epoch());

  std::string lastKmsg = util::read_file("/sys/fs/pstore/console-ramoops");
  boot.setLastKmsg(capnp::Data::Reader((const kj::byte*)lastKmsg.data(), lastKmsg.size()));

  std::string lastPmsg = util::read_file("/sys/fs/pstore/pmsg-ramoops-0");
  boot.setLastPmsg(capnp::Data::Reader((const kj::byte*)lastPmsg.data(), lastPmsg.size()));

  std::string launchLog = util::read_file("/tmp/launch_log");
  boot.setLaunchLog(capnp::Text::Reader(launchLog.data(), launchLog.size()));
  return capnp::messageToFlatArray(msg);
}

kj::Array<capnp::word> logger_build_init_data() {
  MessageBuilder msg;
  auto init = msg.initEvent().initInitData();

  if (util::file_exists("/EON")) {
    init.setDeviceType(cereal::InitData::DeviceType::NEO);
  } else if (util::file_exists("/TICI")) {
    init.setDeviceType(cereal::InitData::DeviceType::TICI);
  } else {
    init.setDeviceType(cereal::InitData::DeviceType::PC);
  }

  init.setVersion(capnp::Text::Reader(COMMA_VERSION));

  std::ifstream cmdline_stream("/proc/cmdline");
  std::vector<std::string> kernel_args;
  std::string buf;
  while (cmdline_stream >> buf) {
    kernel_args.push_back(buf);
  }

  auto lkernel_args = init.initKernelArgs(kernel_args.size());
  for (int i=0; i<kernel_args.size(); i++) {
    lkernel_args.set(i, kernel_args[i]);
  }

  init.setKernelVersion(util::read_file("/proc/version"));

#ifdef QCOM
  {
    std::vector<std::pair<std::string, std::string> > properties;
    property_list(append_property, (void*)&properties);

    auto lentries = init.initAndroidProperties().initEntries(properties.size());
    for (int i=0; i<properties.size(); i++) {
      auto lentry = lentries[i];
      lentry.setKey(properties[i].first);
      lentry.setValue(properties[i].second);
    }
  }
#endif

  const char* dongle_id = getenv("DONGLE_ID");
  if (dongle_id) {
    init.setDongleId(std::string(dongle_id));
  }
  init.setDirty(!getenv("CLEAN"));

  // log params
  Params params = Params();
  init.setGitCommit(params.get("GitCommit"));
  init.setGitBranch(params.get("GitBranch"));
  init.setGitRemote(params.get("GitRemote"));
  init.setPassive(params.read_db_bool("Passive"));
  {
    std::map<std::string, std::string> params_map;
    params.read_db_all(&params_map);
    auto lparams = init.initParams().initEntries(params_map.size());
    int i = 0;
    for (auto& kv : params_map) {
      auto lentry = lparams[i];
      lentry.setKey(kv.first);
      lentry.setValue(kv.second);
      i++;
    }
  }
  return capnp::messageToFlatArray(msg);
}

void log_init_data(LoggerState *s) {
  auto bytes = s->init_data.asBytes();
  logger_log(s, bytes.begin(), bytes.size(), s->has_qlog);
}


static void log_sentinel(LoggerState *s, cereal::Sentinel::SentinelType type) {
  MessageBuilder msg;
  auto sen = msg.initEvent().initSentinel();
  sen.setType(type);
  auto bytes = msg.toBytes();

  logger_log(s, bytes.begin(), bytes.size(), true);
}

// ***** logging functions *****

void logger_init(LoggerState *s, const char* log_name, bool has_qlog) {
  umask(0);

  time_t rawtime = time(NULL);
  struct tm timeinfo;
  localtime_r(&rawtime, &timeinfo);
  strftime(route_name, sizeof(route_name), "%Y-%m-%d--%H-%M-%S", &timeinfo);
}

std::string Logger::next(const std::string& root_path, int* out_part) {
  bool is_start_of_route = !cur_handle;
  if (!is_start_of_route) log_sentinel(this, cereal::Sentinel::SentinelType::END_OF_SEGMENT);

  ++part;

  segment_path = util::string_format("%s/%s--%d", root_path.c_str(), route_name, part);
  cur_handle = std::make_shared<LoggerHandle>(segment_path, log_name, has_qlog);

  if (init_data.size() > 0) {
    auto bytes = init_data.asBytes();
    cur_handle->write(bytes.begin(), bytes.size(), has_qlog);
  }

  if (out_part) {
    *out_part = part;
  }

  log_sentinel(this, is_start_of_route ? cereal::Sentinel::SentinelType::START_OF_ROUTE : cereal::Sentinel::SentinelType::START_OF_SEGMENT);
  return segment_path;
}

std::shared_ptr<LoggerHandle> Logger::get_handle() {
  return cur_handle;
}

void Logger::write(uint8_t* data, size_t data_size, bool in_qlog) {
  if (cur_handle) {
    cur_handle->write(data, data_size, in_qlog);
  }
}

Logger::~Logger() {
  log_sentinel(this, cereal::Sentinel::SentinelType::END_OF_ROUTE);
  cur_handle = nullptr;
}

// LoggerHandle

LoggerHandle::LoggerHandle(const std::string& segment_path, const std::string& log_name, bool has_qlog) {
  const std::string log_path = util::string_format("%s/%s.bz2", segment_path.c_str(), log_name.c_str());
  const std::string qlog_path = segment_path + "/qlog.bz2";
  lock_path = log_path + ".lock";
  
  int err = mkpath((char *)log_path.c_str());
  assert(err == 0);

  FILE* lock_file = fopen(lock_path.c_str(), "wb");
  assert(lock_file != nullptr);
  fclose(lock_file);

  int bzerror;
  log_file = fopen(log_path.c_str(), "wb");
  assert(log_file != nullptr);
  bz_file = BZ2_bzWriteOpen(&bzerror, log_file, 9, 0, 30);
  assert(bzerror == BZ_OK);

  if (has_qlog) {
    qlog_file = fopen(qlog_path.c_str(), "wb");
    assert(qlog_file != nullptr);
    bz_qlog = BZ2_bzWriteOpen(&bzerror, qlog_file, 9, 0, 30);
    assert(bzerror == BZ_OK);
  }
}

void LoggerHandle::write(uint8_t* data, size_t data_size, bool in_qlog) {
  std::scoped_lock lk(lock);
  int bzerror;
  BZ2_bzWrite(&bzerror, bz_file, data, data_size);
  if (in_qlog && bz_qlog != NULL) {
    BZ2_bzWrite(&bzerror, bz_qlog, data, data_size);
  }
}

LoggerHandle::~LoggerHandle() {
  int bzerror;
  BZ2_bzWriteClose(&bzerror, bz_file, 0, NULL, NULL);
  fclose(log_file);

  if (bz_qlog){
    BZ2_bzWriteClose(&bzerror, bz_qlog, 0, NULL, NULL);
    fclose(qlog_file);
  }

  unlink(lock_path.c_str());
}
