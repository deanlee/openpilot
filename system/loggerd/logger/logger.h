#pragma once

#include <unistd.h>

#include <atomic>
#include <cassert>
#include <cerrno>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>

#include "cereal/messaging/messaging.h"
#include "cereal/services.h"
#include "cereal/visionipc/visionipc.h"
#include "cereal/visionipc/visionipc_client.h"
#include "system/camerad/cameras/camera_common.h"
#include "common/params.h"
#include "common/swaglog.h"
#include "common/timing.h"
#include "common/util.h"
#include "system/hardware/hw.h"

#include "system/loggerd/encoder/encoder.h"
#include "system/loggerd/logger/logger_writer.h"
#include "system/loggerd/logger/video_writer.h"
#ifdef QCOM2
#include "system/loggerd/encoder/v4l_encoder.h"
#define Encoder V4LEncoder
#else
#include "system/loggerd/encoder/ffmpeg_encoder.h"
#define Encoder FfmpegEncoder
#endif

constexpr int MAIN_FPS = 20;
const int MAIN_BITRATE = 10000000;

#define NO_CAMERA_PATIENCE 500 // fall back to time-based rotation if all cameras are dead

const bool LOGGERD_TEST = getenv("LOGGERD_TEST");
const int SEGMENT_LENGTH = LOGGERD_TEST ? atoi(getenv("LOGGERD_SEGMENT_LENGTH")) : 60;

class EncoderInfo {
public:
  const char *publish_name;
  const char *filename;
  bool record = true;
  int frame_width = 1928;
  int frame_height = 1208;
  int fps = MAIN_FPS;
  int bitrate = MAIN_BITRATE;
  cereal::EncodeIndex::Type encode_type = cereal::EncodeIndex::Type::FULL_H_E_V_C;
  ::cereal::EncodeData::Reader (cereal::Event::Reader::*get_encode_data_func)() const;
  void (cereal::Event::Builder::*set_encode_idx_func)(::cereal::EncodeIndex::Reader);
};

class LogCameraInfo {
public:
  const char *thread_name;
  int fps = MAIN_FPS;
  CameraType type;
  VisionStreamType stream_type;
  std::vector<EncoderInfo> encoder_infos;
};

const EncoderInfo main_road_encoder_info = {
  .publish_name = "roadEncodeData",
  .filename = "fcamera.hevc",
  .get_encode_data_func = &cereal::Event::Reader::getRoadEncodeData,
  .set_encode_idx_func = &cereal::Event::Builder::setRoadEncodeIdx,
};
const EncoderInfo main_wide_road_encoder_info = {
  .publish_name = "wideRoadEncodeData",
  .filename = "ecamera.hevc",
  .get_encode_data_func = &cereal::Event::Reader::getWideRoadEncodeData,
  .set_encode_idx_func = &cereal::Event::Builder::setWideRoadEncodeIdx,
};
const EncoderInfo main_driver_encoder_info = {
   .publish_name = "driverEncodeData",
  .filename = "dcamera.hevc",
  .record = Params().getBool("RecordFront"),
  .get_encode_data_func = &cereal::Event::Reader::getDriverEncodeData,
  .set_encode_idx_func = &cereal::Event::Builder::setDriverEncodeIdx,
};

const EncoderInfo qcam_encoder_info = {
  .publish_name = "qRoadEncodeData",
  .filename = "qcamera.ts",
  .bitrate = 256000,
  .encode_type = cereal::EncodeIndex::Type::QCAMERA_H264,
  .frame_width = 526,
  .frame_height = 330,
  .get_encode_data_func = &cereal::Event::Reader::getQRoadEncodeData,
  .set_encode_idx_func = &cereal::Event::Builder::setQRoadEncodeIdx,
};


const LogCameraInfo road_camera_info{
    .thread_name = "road_cam_encoder",
    .type = RoadCam,
    .stream_type = VISION_STREAM_ROAD,
    .encoder_infos = {main_road_encoder_info, qcam_encoder_info}
    };

const LogCameraInfo wide_road_camera_info{
    .thread_name = "wide_road_cam_encoder",
    .type = WideRoadCam,
    .stream_type = VISION_STREAM_WIDE_ROAD,
   .encoder_infos = {main_wide_road_encoder_info}
    };
  
const LogCameraInfo driver_camera_info{
    .thread_name = "driver_cam_encoder",
    .type = DriverCam,
    .stream_type = VISION_STREAM_DRIVER,
    .encoder_infos = {main_driver_encoder_info}
    };

const LogCameraInfo cameras_logged[] = {road_camera_info, wide_road_camera_info, driver_camera_info};

struct LoggerdState {
  LoggerState logger = {};
  char segment_path[4096];
  std::atomic<int> rotate_segment;
  std::atomic<double> last_camera_seen_tms;
  std::atomic<int> ready_to_rotate;  // count of encoders ready to rotate
  int max_waiting = 0;
  double last_rotate_tms = 0.;      // last rotate time in ms
};

struct RemoteEncoder {
  std::unique_ptr<VideoWriter> writer;
  int encoderd_segment_offset;
  int current_segment = -1;
  std::vector<Message *> q;
  int dropped_frames = 0;
  bool recording = false;
  bool marked_ready_to_rotate = false;
  bool seen_first_packet = false;
};

void logger_rotate(LoggerdState *s);
void rotate_if_needed(LoggerdState *s);
int handle_encoder_msg(LoggerdState *s, Message *msg, std::string &name, struct RemoteEncoder &re, const EncoderInfo &encoder_info);
void loggerd_thread();
