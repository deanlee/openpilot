#include "tools/replay/camera.h"

#include <capnp/dynamic.h>
#include <cassert>

#include "third_party/linux/include/msm_media_info.h"
#include "tools/replay/util.h"

const int BUFFER_COUNT = 40;

std::tuple<size_t, size_t, size_t> get_nv12_info(int width, int height) {
  int nv12_width = VENUS_Y_STRIDE(COLOR_FMT_NV12, width);
  int nv12_height = VENUS_Y_SCANLINES(COLOR_FMT_NV12, height);
  assert(nv12_width == VENUS_UV_STRIDE(COLOR_FMT_NV12, width));
  assert(nv12_height / 2 == VENUS_UV_SCANLINES(COLOR_FMT_NV12, height));
  size_t nv12_buffer_size = 2346 * nv12_width;  // comes from v4l2_format.fmt.pix_mp.plane_fmt[0].sizeimage
  return {nv12_width, nv12_height, nv12_buffer_size};
}

CameraServer::CameraServer(const std::array<std::pair<int, int>, MAX_CAMERAS> &camera_size) {
  for (int i = 0; i < MAX_CAMERAS; ++i) {
    cameras_[i].stream_type = VisionStreamType(i);
    std::tie(cameras_[i].width, cameras_[i].height) = camera_size[i];
  }
  startVipcServer();
}

CameraServer::~CameraServer() {
  for (auto &cam : cameras_) {
    if (cam.thread.joinable()) {
      {
        std::unique_lock lock(cam.mtx);
        cam.exit = true;
      }
      cam.cv.notify_one();
      cam.thread.join();
    }
  }
  vipc_server_.reset(nullptr);
}

void CameraServer::startVipcServer() {
  vipc_server_.reset(new VisionIpcServer("camerad"));
  for (auto &cam : cameras_) {
    cam.cached_buf.clear();
    cam.frame_reader = nullptr;
    cam.event = nullptr;

    if (cam.width > 0 && cam.height > 0) {
      rInfo("camera[%d] frame size %dx%d", cam.stream_type, cam.width, cam.height);
      auto [nv12_width, nv12_height, nv12_buffer_size] = get_nv12_info(cam.width, cam.height);
      vipc_server_->create_buffers_with_sizes(cam.stream_type, BUFFER_COUNT, false, cam.width, cam.height,
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
    std::unique_lock<std::mutex> lock(cam.mtx);
    cam.cv.wait(lock, [&cam] { return cam.event != nullptr || cam.exit; });
    if (cam.exit) break;

    capnp::FlatArrayMessageReader reader(cam.event->data);
    auto evt = reader.getRoot<cereal::Event>();
    auto eidx = capnp::AnyStruct::Reader(evt).getPointerSection()[0].getAs<cereal::EncodeIndex>();
    int segment_id = eidx.getSegmentId();
    uint32_t frame_id = eidx.getFrameId();
    if (auto yuv = getFrame(cam, segment_id, frame_id)) {
      VisionIpcBufExtra extra = {
          .frame_id = frame_id,
          .timestamp_sof = eidx.getTimestampSof(),
          .timestamp_eof = eidx.getTimestampEof(),
      };
      vipc_server_->send(yuv, &extra);
    } else {
      rError("camera[%d] failed to get frame: %lu", cam.stream_type, segment_id);
    }

    // Prefetch the next frame
    getFrame(cam, segment_id + 1, frame_id + 1);

    cam.event = nullptr;
    cam.cv.notify_one();
  }
}

VisionBuf *CameraServer::getFrame(Camera &cam, int32_t segment_id, uint32_t frame_id) {
  // Check if the frame is cached
  auto buf_it = std::find_if(cam.cached_buf.begin(), cam.cached_buf.end(),
                             [frame_id](VisionBuf *buf) { return buf->get_frame_id() == frame_id; });
  if (buf_it != cam.cached_buf.end()) return *buf_it;

  VisionBuf *yuv_buf = vipc_server_->get_buffer(cam.stream_type);
  if (cam.frame_reader->get(segment_id, yuv_buf)) {
    yuv_buf->set_frame_id(frame_id);
    cam.cached_buf.insert(yuv_buf);
    return yuv_buf;
  }
  return nullptr;
}

void CameraServer::pushFrame(CameraType type, FrameReader *fr, const Event *event) {
  auto &cam = cameras_[type];
  if (cam.width != fr->width || cam.height != fr->height) {
    cam.width = fr->width;
    cam.height = fr->height;
    waitForSent();
    startVipcServer();
  }

  {
    std::unique_lock lock(cam.mtx);
    cam.cv.wait(lock, [&cam] { return !cam.event; });
    cam.frame_reader = fr;
    cam.event = event;
  }
  cam.cv.notify_one();
}

void CameraServer::waitForSent() {
  for (auto &cam : cameras_) {
    std::unique_lock<std::mutex> lock(cam.mtx);
    cam.cv.wait(lock, [&cam]() { return !cam.event; });
  }
}
