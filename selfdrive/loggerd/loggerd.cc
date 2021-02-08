#include <cassert>
#include <sys/resource.h>

#include <ftw.h>
#include <algorithm>
#include "common/timing.h"
#include "common/params.h"
#include "common/swaglog.h"
#include "common/util.h"
#include "camerad/cameras/camera_common.h"
#include "logger.h"
#include "services.h"

#include "visionipc.h"
#include "visionipc_client.h"
#include "encoder.h"
#if defined(QCOM) || defined(QCOM2)
#include "omx_encoder.h"
#define Encoder OmxEncoder
#else
#include "raw_logger.h"
#define Encoder RawLogger
#endif

namespace {

constexpr int MAIN_BITRATE = 5000000;
constexpr int MAIN_FPS = 20;
#ifndef QCOM2
const bool IS_QCOM2 = false;
constexpr int DCAM_BITRATE = 2500000;
#else
const bool IS_QCOM2 = true;
constexpr int DCAM_BITRATE = MAIN_BITRATE;
#endif

#define NO_CAMERA_PATIENCE 500 // fall back to time-based rotation if all cameras are dead

const int SEGMENT_LENGTH = getenv("LOGGERD_TEST") ? atoi(getenv("LOGGERD_SEGMENT_LENGTH")) : 60;

ExitHandler do_exit;

LogCameraInfo cameras_logged[] = {
  {.id = F_CAMERA,
    .stream_type = VISION_STREAM_YUV_BACK,
    .filename = "fcamera.hevc",
    .frame_packet_name = "frame",
    .fps = MAIN_FPS,
    .bitrate = MAIN_BITRATE,
    .is_h265 = true,
    .downscale = false,
    .has_qcamera = true},
  {.id = D_CAMERA,
    .stream_type = VISION_STREAM_YUV_FRONT,
    .filename = "dcamera.hevc",
    .frame_packet_name = "frontFrame",
    .fps = MAIN_FPS, // on EONs, more compressed this way
    .bitrate = DCAM_BITRATE,
    .is_h265 = true,
    .downscale = false,
    .has_qcamera = false},
#ifdef QCOM2
  {.id = E_CAMERA,
    .stream_type = VISION_STREAM_YUV_WIDE,
    .filename = "ecamera.hevc",
    .frame_packet_name = "wideFrame",
    .fps = MAIN_FPS,
    .bitrate = MAIN_BITRATE,
    .is_h265 = true,
    .downscale = false,
    .has_qcamera = false},
#endif
};
const LogCameraInfo qcam_info = {
    .filename = "qcamera.ts",
    .fps = MAIN_FPS,
    .bitrate = 128000,
    .is_h265 = false,
    .downscale = true,
#ifndef QCOM2
    .frame_width = 480, .frame_height = 360
#else
    .frame_width = 526, .frame_height = 330 // keep pixel count the same?
#endif
};


typedef struct QlogState {
  int counter, freq;
} QlogState;

class EncoderState {
public:
  EncoderState(const LogCameraInfo &ci, SubSocket *sock, const QlogState &qs, bool need_waiting)
      : ci(ci), frame_sock(sock), qlog_state(qs), need_waiting(need_waiting) {
    last_camera_seen_tms = millis_since_boot();
    thread = std::thread(&EncoderState::encoder_thread, this);
  }
  ~EncoderState() {
    thread.join();
  }
  void rotate_if_needed();
  void encoder_thread();
  LogCameraInfo ci;
  int segment = -1;
  const bool need_waiting;
  uint32_t total_frame_cnt = 0;
  LoggerHandle *lh = NULL;
  QlogState qlog_state;

  std::thread thread;
  std::unique_ptr<SubSocket> frame_sock;
  std::atomic<double> last_camera_seen_tms;
  std::vector<Encoder *> encoders;
};

struct LoggerdState {
  LoggerState logger = {};
  char segment_path[PATH_MAX] = {};
  int encoders_max_waiting = 0;
  int rotate_segment = -1;
  double last_rotate_tms;
  std::mutex rotate_lock;
  std::atomic<int> encoders_waiting;
  std::condition_variable cv;
  std::vector<EncoderState *> encoder_states;
};
LoggerdState s;

void drain_socket(LoggerHandle *lh, SubSocket *sock, QlogState &qs) {
  Message *msg = nullptr;
  while (!do_exit && (msg = sock->receive(true))) {
    lh_log(lh, (uint8_t *)msg->getData(), msg->getSize(), qs.counter == 0 && qs.freq != -1);
    if (qs.freq != -1) {
      qs.counter = (qs.counter + 1) % qs.freq;
    }
    delete msg;
  }
}

int clear_locks_fn(const char* fpath, const struct stat *sb, int tyupeflag) {
  const char* dot = strrchr(fpath, '.');
  if (dot && strcmp(dot, ".lock") == 0) {
    unlink(fpath);
  }
  return 0;
}

void clear_locks() {
  ftw(LOG_ROOT.c_str(), clear_locks_fn, 16);
}

void loggerd_should_rotate() {
  bool should_rotate = s.rotate_segment == -1 || s.encoders_waiting >= s.encoders_max_waiting;
  if (!should_rotate) {
    if (double tms = millis_since_boot(); (tms - s.last_rotate_tms) >= (SEGMENT_LENGTH * 1000)) {
      for (auto &es : s.encoder_states) {
        if (es->need_waiting &&  (tms - es->last_camera_seen_tms) >= NO_CAMERA_PATIENCE) {
          should_rotate = true;
          LOGW("no camera %d packet seen. auto rotated", es->ci.id);
          break;
        }
      }
    }
  }
  if (should_rotate && !do_exit) {
    {  // rotate to new segment
      std::unique_lock lk(s.rotate_lock);
      s.last_rotate_tms = millis_since_boot();
      assert(0 == logger_next(&s.logger, LOG_ROOT.c_str(), s.segment_path, sizeof(s.segment_path), &s.rotate_segment));
      s.encoders_waiting = 0;
    }
    s.cv.notify_all();
    LOGW((s.rotate_segment == 0) ? "logging to %s" : "rotated to %s", s.segment_path);
  }
}

void EncoderState::rotate_if_needed() {
  const int max_segment_frames = SEGMENT_LENGTH * MAIN_FPS;
  bool should_rotate = false;
  std::string segment_path;
  {
    std::unique_lock lk(s.rotate_lock);
    last_camera_seen_tms = millis_since_boot();
    // rotate the encoder if the logger is on a newer segment
    should_rotate = segment == -1 || (segment != s.rotate_segment);
    if (!should_rotate && need_waiting && ((total_frame_cnt % max_segment_frames) == 0)) {
      // max_segment_frames have been recorded, need to rotate
      should_rotate = true;
      s.encoders_waiting++;
    }
    // wait logger rotated
    if (should_rotate) {
      s.cv.wait(lk, [&] { return s.rotate_segment > segment || do_exit; });
      segment = s.rotate_segment;
      segment_path = s.segment_path;
    }
  }
  if (should_rotate && !do_exit) {
    LOGW("camera %d rotate encoder to %s", ci.id, segment_path.c_str());
    if (lh) {
      lh_close(lh);
      lh = nullptr;
    }
    lh = logger_get_handle(&s.logger);
    
    for (auto &e : encoders) {
      e->encoder_close();
      e->encoder_open(segment_path.c_str());
    }
  }
}

void EncoderState::encoder_thread() {
  set_thread_name(ci.filename);

  VisionIpcClient vipc_client = VisionIpcClient("camerad", ci.stream_type, false);
  while (!vipc_client.connect(false)) {
    if (do_exit) return;
    util::sleep_for(100);
  }

  // init encoders
  if (encoders.empty()) {
    VisionBuf buf_info = vipc_client.buffers[0];
    LOGD("encoder init %dx%d", buf_info.width, buf_info.height);
    // main encoder
    encoders.push_back(new Encoder(ci.filename, buf_info.width, buf_info.height,
                                    ci.fps, ci.bitrate, ci.is_h265, ci.downscale));
    // qcamera encoder
    if (ci.has_qcamera) {
      encoders.push_back(new Encoder(qcam_info.filename, qcam_info.frame_width, qcam_info.frame_height,
                                      qcam_info.fps, qcam_info.bitrate, qcam_info.is_h265, qcam_info.downscale));
    }
  }

  while (!do_exit) {
    VisionIpcBufExtra extra;
    VisionBuf* buf = vipc_client.recv(&extra);
    if (buf == nullptr) continue;

    rotate_if_needed();
    if (do_exit) break;

    // log frame socket
    drain_socket(lh, frame_sock.get(), qlog_state);

    // encode a frame
    for (int i = 0; i < encoders.size(); ++i) {
      int out_id = encoders[i]->encode_frame(buf->y, buf->u, buf->v,
                                              buf->width, buf->height, extra.timestamp_eof);
      if (i == 0 && out_id != -1) {
        // publish encode index
        MessageBuilder msg;
        // this is really ugly
        auto eidx = ci.id == D_CAMERA ? msg.initEvent().initFrontEncodeIdx() : (ci.id == E_CAMERA ? msg.initEvent().initWideEncodeIdx() : msg.initEvent().initEncodeIdx());
        eidx.setFrameId(extra.frame_id);
        eidx.setTimestampSof(extra.timestamp_sof);
        eidx.setTimestampEof(extra.timestamp_eof);
        eidx.setType((IS_QCOM2 || ci.id != D_CAMERA) ? cereal::EncodeIndex::Type::FULL_H_E_V_C : cereal::EncodeIndex::Type::FRONT);
        eidx.setEncodeId(total_frame_cnt);
        eidx.setSegmentNum(segment);
        eidx.setSegmentId(out_id);
        auto bytes = msg.toBytes();
        lh_log(lh, bytes.begin(), bytes.size(), false);
      }
    }
    ++total_frame_cnt;
  }

  if (lh) lh_close(lh);

  LOG("encoder destroy");
  for(auto &e : encoders) {
    e->encoder_close();
    delete e;
  }
}

} // namespace

int main(int argc, char** argv) {
  setpriority(PRIO_PROCESS, 0, -12);

  clear_locks();

  // setup messaging
  std::map<SubSocket*, QlogState> qlog_states;
  Context *ctx = Context::create();
  Poller * poller = Poller::create();
  const bool record_front = Params().read_db_bool("RecordFront");
  // subscribe to all socks
  for (const auto& it : services) {
    if (!it.should_log) continue;

    SubSocket * sock = SubSocket::create(ctx, it.name);
    assert(sock != NULL);
    QlogState qs = {.counter = 0, .freq = it.decimation};

    auto camra_info = std::find_if(std::begin(cameras_logged), std::end(cameras_logged), [&](LogCameraInfo &ci) {
      return strcmp(it.name, ci.frame_packet_name) == 0 && (ci.id != D_CAMERA || record_front);
    });
    if (camra_info != std::end(cameras_logged)) {
      // init and start encoder thread
      const bool need_waiting = (IS_QCOM2 || camra_info->id != D_CAMERA);
      s.encoders_max_waiting += need_waiting;
      s.encoder_states.push_back(new EncoderState(*camra_info, sock, qs, need_waiting));
    } else {
      poller->registerSocket(sock);
      qlog_states[sock] = qs;
    }
  }
  
  logger_init(&s.logger, "rlog", true);
  while (!do_exit) {
    loggerd_should_rotate();

    for (auto sock : poller->poll(1000)) {
      drain_socket(s.logger.cur_handle, sock, qlog_states[sock]);
    }
  }

  LOGW("closing encoders");
  // stop encoder threads
  s.encoders_waiting = 0;
  s.cv.notify_all();

  for (auto &[sock, qs] : qlog_states) delete sock;
  for (auto &e : s.encoder_states) { delete e; }

  LOGW("closing logger");
  logger_close(&s.logger);

  delete poller;
  delete ctx;

  return 0;
}
