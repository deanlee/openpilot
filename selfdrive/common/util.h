#pragma once

#include <unistd.h>
#include <fcntl.h>
#include <string>
#include <memory>

void set_thread_name(const char* name);
int set_realtime_priority(int level);
int set_core_affinity(int core);

namespace util {

// ***** math helpers *****

// map x from [a1, a2] to [b1, b2]
template<typename T>
T map_val(T x, T a1, T a2, T b1, T b2) {
  if (x < a1) x = a1;
  else if (x > a2) x = a2;

  T ra = a2 - a1;
  T rb = b2 - b1;
  return (x - a1)*rb / ra + b1;
}

// ***** string helpers *****

template <typename... Args>
inline std::string string_format(const std::string& format, Args... args) {
  size_t size = snprintf(nullptr, 0, format.c_str(), args...) + 1;
  std::unique_ptr<char[]> buf(new char[size]);
  snprintf(buf.get(), size, format.c_str(), args...);
  return std::string(buf.get(), buf.get() + size - 1);
}

std::string read_file(const std::string& fn);
int write_file(const char* path, const void* data, size_t size, int flags = O_WRONLY, mode_t mode = 0777);
std::string readlink(const std::string& path);
std::string getenv_default(const char* env_var, const char* suffix, const char* default_val);
void sleep_for(const int milliseconds);
bool file_exists(const std::string& fn);

}  // namespace util

class ExitHandler {
public:
  ExitHandler();
  ~ExitHandler();
  operator bool();
  ExitHandler& operator=(bool v);
  bool power_failure();

private:
  static void set_do_exit(int sig);
  struct PrivateData;
  inline static PrivateData *d = nullptr;
};

struct unique_fd {
  unique_fd(int fd = -1) : fd_(fd) {}
  unique_fd& operator=(unique_fd&& uf) {
    fd_ = uf.fd_;
    uf.fd_ = -1;
    return *this;
  }
  ~unique_fd() {
    if (fd_ != -1) close(fd_);
  }
  operator int() const { return fd_; }
  int fd_;
};

class FirstOrderFilter {
public:
  FirstOrderFilter(float x0, float ts, float dt) {
    k_ = (dt / ts) / (1.0 + dt / ts);
    x_ = x0;
  }
  inline float update(float x) {
    x_ = (1. - k_) * x_ + k_ * x;
    return x_;
  }
  inline void reset(float x) { x_ = x; }

private:
  float x_, k_;
};
