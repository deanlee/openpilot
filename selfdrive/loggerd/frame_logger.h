#pragma once

#include <cstdint>
#include <mutex>
#include <string>

#include "common/visionipc.h"
class FrameLogger {
 public:
  FrameLogger(const std::string &afilename, int awidth, int aheight, int afps)
      : filename(afilename), width(awidth), height(aheight), fps(afps) {}
  virtual ~FrameLogger() { closeFile(); }

  int LogFrame(uint64_t ts, const uint8_t *y_ptr, const uint8_t *u_ptr, const uint8_t *v_ptr, int width, int height, VIPCBufExtra *extra) {
    if (rotating) {
      closeFile();

      // create camera lock file
      lock_path = util::string_format("%s/%s.lock", next_path.c_str(), filename.c_str());
      int lock_fd = open(lock_path.c_str(), O_RDWR | O_CREAT, 0777);
      assert(lock_fd >= 0);
      close(lock_fd);

      std::string vid_path = util::string_format("%s/%s", next_path.c_str(), filename.c_str());
      Open(vid_path);

      counter = 0;
      rotating = false;
      is_open = true;
    }

    if (!is_open) return -1;

    if (ProcessFrame(ts, y_ptr, u_ptr, v_ptr, width, height, extra)) {
      counter++;
    }
    return counter;
  }

  void Rotate(const std::string &new_path, int new_segment) {
    next_path = new_path;
    rotating = true;
  }

 protected:
  virtual void Open(const std::string &path) = 0;
  virtual void Close() = 0;
  virtual bool ProcessFrame(uint64_t ts, const uint8_t *y_ptr, const uint8_t *u_ptr, const uint8_t *v_ptr, int width, int height, VIPCBufExtra *extra) = 0;

  bool is_open = false;
  int width, height, fps;
  int counter = 0;

 private:
  void closeFile() {
    if (is_open) {
      Close();
      unlink(lock_path.c_str());
      is_open = false;
    }
  }
  std::string next_path, filename, lock_path;
  bool rotating = false;
};
