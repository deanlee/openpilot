#pragma once

#include <memory>
#include <mutex>
#include <set>
#include <tuple>
#include <utility>

#include "cereal/visionipc/visionipc_server.h"
#include "tools/replay/framereader.h"
#include "tools/replay/logreader.h"

std::tuple<size_t, size_t, size_t> get_nv12_info(int width, int height);

class CameraServer {
public:
  CameraServer(const std::array<std::pair<int, int>, MAX_CAMERAS> &camera_size = {});
  ~CameraServer();
  void pushFrame(CameraType type, FrameReader* fr, const Event *event);
  void waitForSent();

protected:
  struct Camera {
    VisionStreamType stream_type;
    int width;
    int height;
    std::thread thread;
    std::mutex mtx;
    std::condition_variable cv;
    bool exit;
    FrameReader *frame_reader;
    const Event *event;
    std::set<VisionBuf *> cached_buf;
  };
  void startVipcServer();
  void cameraThread(Camera &cam);
  VisionBuf *getFrame(Camera &cam, int32_t segment_id, uint32_t frame_id);

  Camera cameras_[MAX_CAMERAS] = {};
  std::unique_ptr<VisionIpcServer> vipc_server_;
};
