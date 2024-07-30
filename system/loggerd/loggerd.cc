#include <sys/xattr.h>

#include <map>
#include <memory>
#include <string>
#include <unordered_map>

#include "common/params.h"
#include "system/loggerd/encoder/encoder.h"
#include "system/loggerd/loggerd.h"
#include "system/loggerd/video_writer.h"

ExitHandler do_exit;

struct LoggerdState {
  LoggerState logger;
  std::atomic<double> last_camera_seen_tms;
  double last_rotate_tms = 0.;      // last rotate time in ms
};

void logger_rotate(LoggerdState *s) {
  bool ret =s->logger.next();
  assert(ret);
  s->last_rotate_tms = millis_since_boot();
  LOGW((s->logger.segment() == 0) ? "logging to %s" : "rotated to %s", s->logger.segmentPath().c_str());
}

void rotate_if_needed(LoggerdState *s) {
  // fallback logic to prevent extremely long segments in the case of camera, encoder, etc. malfunctions
  double tms = millis_since_boot();
  double seg_length_secs = (tms - s->last_rotate_tms) / 1000.;
  if ((seg_length_secs > SEGMENT_LENGTH) && !LOGGERD_TEST) {
    // TODO: might be nice to put these reasons in the sentinel
    if ((tms - s->last_camera_seen_tms) > NO_CAMERA_PATIENCE) {
      LOGE("no camera packets seen. auto rotating");
      logger_rotate(s);
    } else if (seg_length_secs > SEGMENT_LENGTH*1.2) {
      LOGE("segment too long. auto rotating");
      logger_rotate(s);
    }
  }
}

struct RemoteEncoder {
  std::unique_ptr<VideoWriter> writer;
  int encoder_seg_num = -1;
  int logger_seg_num = -1;
};

int handle_encoder_msg(LoggerdState *s, Message *msg, std::string &name, struct RemoteEncoder &re, const EncoderInfo &encoder_info) {
  capnp::FlatArrayMessageReader cmsg(kj::ArrayPtr<capnp::word>((capnp::word *)msg->getData(), msg->getSize() / sizeof(capnp::word)));
  auto event = cmsg.getRoot<cereal::Event>();
  auto edata = (event.*(encoder_info.get_encode_data_func))();
  auto idx = edata.getIdx();
  int seg_num = idx.getSegmentNum();

  if (re.encoder_seg_num != seg_num) {
    re.writer.reset();
    // rotate the logger to next section if encoder is in the same section with logger.
    if (re.logger_seg_num == s->logger.segment()) {
      logger_rotate(s);
    }
    re.encoder_seg_num = seg_num;
  }
  re.logger_seg_num = s->logger.segment();

  if (!re.writer && encoder_info.record) {
    auto header = edata.getHeader();
    re.writer = std::make_unique<VideoWriter>(s->logger.segmentPath().c_str(),
                                              encoder_info.filename, idx.getType() != cereal::EncodeIndex::Type::FULL_H_E_V_C,
                                              edata.getWidth(), edata.getHeight(), encoder_info.fps, idx.getType());
    re.writer->write((uint8_t *)header.begin(), header.size(), idx.getTimestampEof() / 1000, true, false);
  }

  if (re.writer) {
    auto data = edata.getData();
    bool key_frame = idx.getFlags() & V4L2_BUF_FLAG_KEYFRAME;
    re.writer->write((uint8_t *)data.begin(), data.size(), idx.getTimestampEof() / 1000, false, key_frame);
  }

  MessageBuilder bmsg;
  auto evt = bmsg.initEvent(event.getValid());
  evt.setLogMonoTime(event.getLogMonoTime());
  (evt.*(encoder_info.set_encode_idx_func))(idx);
  auto new_msg = bmsg.toBytes();
  s->logger.write((uint8_t *)new_msg.begin(), new_msg.size(), true);  // always in qlog?
  return new_msg.size();
}

void handle_user_flag(LoggerdState *s) {
  static int prev_segment = -1;
  if (s->logger.segment() == prev_segment) return;

  LOGW("preserving %s", s->logger.segmentPath().c_str());

#ifdef __APPLE__
  int ret = setxattr(s->logger.segmentPath().c_str(), PRESERVE_ATTR_NAME, &PRESERVE_ATTR_VALUE, 1, 0, 0);
#else
  int ret = setxattr(s->logger.segmentPath().c_str(), PRESERVE_ATTR_NAME, &PRESERVE_ATTR_VALUE, 1, 0);
#endif
  if (ret) {
    LOGE("setxattr %s failed for %s: %s", PRESERVE_ATTR_NAME, s->logger.segmentPath().c_str(), strerror(errno));
  }

  // mark route for uploading
  Params params;
  std::string routes = Params().get("AthenadRecentlyViewedRoutes");
  params.put("AthenadRecentlyViewedRoutes", routes + "," + s->logger.routeName());

  prev_segment = s->logger.segment();
}

void loggerd_thread() {
  // setup messaging
  typedef struct ServiceState {
    std::string name;
    int counter, freq;
    bool encoder, user_flag;
  } ServiceState;
  std::unordered_map<SubSocket*, ServiceState> service_state;
  std::unordered_map<SubSocket*, struct RemoteEncoder> remote_encoders;

  std::unique_ptr<Context> ctx(Context::create());
  std::unique_ptr<Poller> poller(Poller::create());

  // subscribe to all socks
  for (const auto& [_, it] : services) {
    const bool encoder = util::ends_with(it.name, "EncodeData");
    const bool livestream_encoder = util::starts_with(it.name, "livestream");
    if (!it.should_log && (!encoder || livestream_encoder)) continue;
    LOGD("logging %s", it.name.c_str());

    SubSocket * sock = SubSocket::create(ctx.get(), it.name);
    assert(sock != NULL);
    poller->registerSocket(sock);
    service_state[sock] = {
      .name = it.name,
      .counter = 0,
      .freq = it.decimation,
      .encoder = encoder,
      .user_flag = it.name == "userFlag",
    };
  }

  LoggerdState s;
  // init logger
  logger_rotate(&s);
  Params().put("CurrentRoute", s.logger.routeName());

  std::map<std::string, EncoderInfo> encoder_infos_dict;
  for (const auto &cam : cameras_logged) {
    for (const auto &encoder_info : cam.encoder_infos) {
      encoder_infos_dict[encoder_info.publish_name] = encoder_info;
    }
  }

  uint64_t msg_count = 0, bytes_count = 0;
  double start_ts = millis_since_boot();
  while (!do_exit) {
    // poll for new messages on all sockets
    for (auto sock : poller->poll(1000)) {
      if (do_exit) break;

      ServiceState &service = service_state[sock];
      if (service.user_flag) {
        handle_user_flag(&s);
      }

      // drain socket
      int count = 0;
      Message *msg = nullptr;
      while (!do_exit && (msg = sock->receive(true))) {
        const bool in_qlog = service.freq != -1 && (service.counter++ % service.freq == 0);
        if (service.encoder) {
          s.last_camera_seen_tms = millis_since_boot();
          bytes_count += handle_encoder_msg(&s, msg, service.name, remote_encoders[sock], encoder_infos_dict[service.name]);
        } else {
          s.logger.write((uint8_t *)msg->getData(), msg->getSize(), in_qlog);
          bytes_count += msg->getSize();
        }
        delete msg;

        rotate_if_needed(&s);

        if ((++msg_count % 1000) == 0) {
          double seconds = (millis_since_boot() - start_ts) / 1000.0;
          LOGD("%" PRIu64 " messages, %.2f msg/sec, %.2f KB/sec", msg_count, msg_count / seconds, bytes_count * 0.001 / seconds);
        }

        count++;
        if (count >= 200) {
          LOGD("large volume of '%s' messages", service.name.c_str());
          break;
        }
      }
    }
  }

  LOGW("closing logger");
  s.logger.setExitSignal(do_exit.signal);

  if (do_exit.power_failure) {
    LOGE("power failure");
    sync();
    LOGE("sync done");
  }

  // messaging cleanup
  for (auto &[sock, service] : service_state) delete sock;
}

int main(int argc, char** argv) {
  if (!Hardware::PC()) {
    int ret;
    ret = util::set_core_affinity({0, 1, 2, 3});
    assert(ret == 0);
    // TODO: why does this impact camerad timings?
    //ret = util::set_realtime_priority(1);
    //assert(ret == 0);
  }

  loggerd_thread();

  return 0;
}
