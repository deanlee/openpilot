
#include "common/util.h"

#include <string.h>
#include <cstdio>
#include <sstream>
#include <thread>
#include <csignal>
#include <chrono>
#include <fstream>
#include <atomic>
#ifdef __linux__
#include <sys/prctl.h>
#include <sys/syscall.h>
#ifndef __USE_GNU
#define __USE_GNU
#endif
#include <sched.h>
#endif // __linux__

void set_thread_name(const char* name) {
#ifdef __linux__
  // pthread_setname_np is dumb (fails instead of truncates)
  prctl(PR_SET_NAME, (unsigned long)name, 0, 0, 0);
#endif
}

int set_realtime_priority(int level) {
#ifdef __linux__
  long tid = syscall(SYS_gettid);

  // should match python using chrt
  struct sched_param sa;
  memset(&sa, 0, sizeof(sa));
  sa.sched_priority = level;
  return sched_setscheduler(tid, SCHED_FIFO, &sa);
#else
  return -1;
#endif
}

int set_core_affinity(int core) {
#ifdef __linux__
  long tid = syscall(SYS_gettid);
  cpu_set_t rt_cpu;

  CPU_ZERO(&rt_cpu);
  CPU_SET(core, &rt_cpu);
  return sched_setaffinity(tid, sizeof(rt_cpu), &rt_cpu);
#else
  return -1;
#endif
}

namespace util {

std::string read_file(const std::string& fn) {
  std::ifstream ifs(fn, std::ios::binary | std::ios::ate);
  if (ifs) {
    std::ifstream::pos_type pos = ifs.tellg();
    if (pos != std::ios::beg) {
      std::string result;
      result.resize(pos);
      ifs.seekg(0, std::ios::beg);
      ifs.read(result.data(), pos);
      if (ifs) {
        return result;
      }
    }
  }
  ifs.close();

  // fallback for files created on read, e.g. procfs
  std::ifstream f(fn);
  std::stringstream buffer;
  buffer << f.rdbuf();
  return buffer.str();
}

int write_file(const char* path, const void* data, size_t size, int flags, mode_t mode) {
  int fd = open(path, flags, mode);
  if (fd == -1) {
    return -1;
  }
  ssize_t n = write(fd, data, size);
  close(fd);
  return (n >= 0 && (size_t)n == size) ? 0 : -1;
}

void sleep_for(const int milliseconds) {
  std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

bool file_exists(const std::string& fn) {
  std::ifstream f(fn);
  return f.good();
}

std::string readlink(const std::string &path) {
  char buff[4096];
  ssize_t len = ::readlink(path.c_str(), buff, sizeof(buff)-1);
  if (len != -1) {
    buff[len] = '\0';
    return std::string(buff);
  }
  return "";
}

std::string getenv_default(const char* env_var, const char * suffix, const char* default_val) {
  const char* env_val = getenv(env_var);
  if (env_val != NULL){
    return std::string(env_val) + std::string(suffix);
  } else {
    return std::string(default_val);
  }
}

}  // namespace util

// class ExitHandler

#ifndef sighandler_t
typedef void (*sighandler_t)(int sig);
#endif

struct ExitHandler::PrivateData {
  std::atomic<bool> do_exit = false;
  std::atomic<bool> power_failure = false;
};

ExitHandler::ExitHandler() {
  if (!d) d = new PrivateData;

  std::signal(SIGINT, (sighandler_t)set_do_exit);
  std::signal(SIGTERM, (sighandler_t)set_do_exit);

#ifndef __APPLE__
  std::signal(SIGPWR, (sighandler_t)set_do_exit);
#endif
}

ExitHandler::~ExitHandler() {
  delete d;
}

ExitHandler::operator bool() { 
  return d->do_exit; 
}

ExitHandler& ExitHandler::operator=(bool v) {
  d->do_exit = v;
  return *this;
}
bool ExitHandler::power_failure() {
  return d->power_failure;
}

void ExitHandler::set_do_exit(int sig) {
#ifndef __APPLE__
  d->power_failure = (sig == SIGPWR);
#endif
  d->do_exit = true;
}
