#pragma once

#include <condition_variable>
#include <memory>
#include <mutex>
#include <set>
#include <tuple>
#include <utility>

#include "msgq/visionipc/visionipc_server.h"
#include "tools/replay/framereader.h"
#include "tools/replay/logreader.h"

std::tuple<size_t, size_t, size_t> get_nv12_info(int width, int height);

class CameraServer {
public:
  CameraServer(std::pair<int, int> camera_size[MAX_CAMERAS] = nullptr);
  ~CameraServer();
  void pushFrame(CameraType type, FrameReader* fr, const Event *event);
  void waitForSent();

protected:
  struct Camera {
    CameraType type;
    VisionStreamType stream_type;
    int width;
    int height;
    std::thread thread;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::pair<FrameReader*, const Event *> item_;
    std::set<VisionBuf *> cached_buf;
  };
  void startVipcServer();
  void cameraThread(Camera &cam);
  VisionBuf *getFrame(Camera &cam, FrameReader *fr, int32_t segment_id, uint32_t frame_id);

  Camera cameras_[MAX_CAMERAS] = {
      {.type = RoadCam, .stream_type = VISION_STREAM_ROAD},
      {.type = DriverCam, .stream_type = VISION_STREAM_DRIVER},
      {.type = WideRoadCam, .stream_type = VISION_STREAM_WIDE_ROAD},
  };
  std::atomic<int> publishing_ = 0;
  std::unique_ptr<VisionIpcServer> vipc_server_;
  std::atomic<bool> exit_ = false;
  std::mutex publishing_mutex_;
  std::condition_variable publishing_cond_;
};
