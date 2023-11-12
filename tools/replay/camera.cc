#include "tools/replay/camera.h"

#include <cassert>
#include "third_party/linux/include/msm_media_info.h"
#include "tools/replay/util.h"

std::tuple<size_t, size_t, size_t> get_nv12_info(int width, int height) {
  int nv12_width = VENUS_Y_STRIDE(COLOR_FMT_NV12, width);
  int nv12_height = VENUS_Y_SCANLINES(COLOR_FMT_NV12, height);
  assert(nv12_width == VENUS_UV_STRIDE(COLOR_FMT_NV12, width));
  assert(nv12_height / 2 == VENUS_UV_SCANLINES(COLOR_FMT_NV12, height));
  size_t nv12_buffer_size = 2346 * nv12_width;  // comes from v4l2_format.fmt.pix_mp.plane_fmt[0].sizeimage
  return {nv12_width, nv12_height, nv12_buffer_size};
}

CameraServer::CameraServer(std::pair<int, int> camera_size[MAX_CAMERAS]) {
  for (int i = 0; i < MAX_CAMERAS; ++i) {
    std::tie(cameras_[i].width, cameras_[i].height) = camera_size[i];
  }
  startVipcServer();
}

CameraServer::~CameraServer() {
  for (auto &cam : cameras_) {
    if (cam.thread.joinable()) {
      cam.queue.push({});
      cam.thread.join();
    }
  }
  vipc_server_.reset(nullptr);
}

void CameraServer::startVipcServer() {
  vipc_server_.reset(new VisionIpcServer("camerad"));
  for (auto &cam : cameras_) {
    if (cam.width > 0 && cam.height > 0) {
      rInfo("camera[%d] frame size %dx%d", cam.type, cam.width, cam.height);
      auto [nv12_width, nv12_height, nv12_buffer_size] = get_nv12_info(cam.width, cam.height);
      vipc_server_->create_buffers_with_sizes(cam.stream_type, YUV_BUFFER_COUNT, false, cam.width, cam.height,
                                              nv12_buffer_size, nv12_width, nv12_width * nv12_height);
      if (!cam.thread.joinable()) {
        cam.thread = std::thread(&CameraServer::cameraThread, this, std::ref(cam));
      }
    }
  }
  vipc_server_->start_listener();
}

void CameraServer::cameraThread(Camera &cam) {
  while (true) {
    const auto [fr, eidx] = cam.queue.pop();
    if (!fr) break;

    VisionBuf *buffer = vipc_server_->get_buffer(cam.stream_type);
    if (fr->get(eidx.getSegmentId(), buffer)) {
      VisionIpcBufExtra extra = {
          .frame_id = eidx.getFrameId(),
          .timestamp_sof = eidx.getTimestampSof(),
          .timestamp_eof = eidx.getTimestampEof(),
      };
      buffer->set_frame_id(eidx.getFrameId());
      vipc_server_->send(buffer, &extra);
    } else {
      rError("camera[%d] failed to get frame: %lu", cam.type, eidx.getSegmentId());
    }

    std::unique_lock lk(mutex_);
    cam.publishing = false;
    cv_.notify_all();
  }
}

void CameraServer::pushFrame(CameraType type, FrameReader *fr, const cereal::EncodeIndex::Reader &eidx) {
  auto &cam = cameras_[type];
  if (cam.width != fr->width || cam.height != fr->height) {
    cam.width = fr->width;
    cam.height = fr->height;
    waitForSent();
    startVipcServer();
  }

  std::unique_lock lk(mutex_);
  cv_.wait(lk, [&cam]() { return cam.publishing == false; });
  cam.publishing = true;
  cam.queue.push({fr, eidx});
}

void CameraServer::waitForSent() {
  std::unique_lock lk(mutex_);
  cv_.wait(lk, [this]() {
    return std::all_of(std::begin(cameras_), std::end(cameras_),
                       [](const auto &c) { return c.publishing == false; });
  });
}
