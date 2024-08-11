#include <sys/xattr.h>

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "common/params.h"
#include "system/loggerd/encoder/encoder.h"
#include "system/loggerd/loggerd.h"
#include "system/loggerd/video_writer.h"

ExitHandler do_exit;

struct RemoteEncoder {
  RemoteEncoder(const EncoderInfo &info) : encoder_info(info) {

  }
  std::unique_ptr<VideoWriter> writer;
  int current_segment = -1;
  int prev_encoder_segment_num = -1;
  std::vector<std::unique_ptr<Message>> q;
  int dropped_frames = 0;
  bool marked_ready_to_rotate = false;
  EncoderInfo encoder_info;
};

struct LoggerdState {
  LoggerState logger;
  std::atomic<double> last_camera_seen_tms{0.0};
  std::atomic<int> ready_to_rotate{0};  // count of encoders ready to rotate
  int max_waiting = 0;
  double last_rotate_tms = 0.;      // last rotate time in ms
  std::unordered_map<SubSocket*, std::unique_ptr<RemoteEncoder>> remote_encoders;
};

void logger_rotate(LoggerdState *s) {
  bool ret =s->logger.next();
  assert(ret);
  s->ready_to_rotate = 0;
  s->last_rotate_tms = millis_since_boot();
  LOGW((s->logger.segment() == 0) ? "logging to %s" : "rotated to %s", s->logger.segmentPath().c_str());
}

void rotate_if_needed(LoggerdState *s) {
  // all encoders ready, trigger rotation
  bool all_ready = s->ready_to_rotate == s->max_waiting;

  // fallback logic to prevent extremely long segments in the case of camera, encoder, etc. malfunctions
  bool timed_out = false;
  double tms = millis_since_boot();
  double seg_length_secs = (tms - s->last_rotate_tms) / 1000.;
  if ((seg_length_secs > SEGMENT_LENGTH) && !LOGGERD_TEST) {
    // TODO: might be nice to put these reasons in the sentinel
    if ((tms - s->last_camera_seen_tms) > NO_CAMERA_PATIENCE) {
      timed_out = true;
      LOGE("no camera packets seen. auto rotating");
    } else if (seg_length_secs > SEGMENT_LENGTH*1.2) {
      timed_out = true;
      LOGE("segment too long. auto rotating");
    }
  }

  if (all_ready || timed_out) {
    logger_rotate(s);
  }
}

size_t write_encode_data(LoggerdState *s, cereal::Event::Reader event, struct RemoteEncoder &re) {
  auto edata = (event.*(re.encoder_info.get_encode_data_func))();
  auto idx = edata.getIdx();
  auto flags = idx.getFlags();

  // if we aren't recording yet, try to start, since we are in the correct segment
  if (!re.writer && re.encoder_info.record) {
    if (flags & V4L2_BUF_FLAG_KEYFRAME) {
      // only create on iframe
      if (re.dropped_frames) {
        // this should only happen for the first segment, maybe
        LOGW("%s: dropped %d non iframe packets before init", re.encoder_info.publish_name, re.dropped_frames);
        re.dropped_frames = 0;
      }
      // if we aren't actually recording, don't create the writer
      assert(re.encoder_info.filename != NULL);
      re.writer.reset(new VideoWriter(s->logger.segmentPath().c_str(),
                                      re.encoder_info.filename, idx.getType() != cereal::EncodeIndex::Type::FULL_H_E_V_C,
                                      edata.getWidth(), edata.getHeight(), re.encoder_info.fps, idx.getType()));
      // write the header
      auto header = edata.getHeader();
      re.writer->write((uint8_t *)header.begin(), header.size(), idx.getTimestampEof() / 1000, true, false);
    } else {
      // this is a sad case when we aren't recording, but don't have an iframe
      // nothing we can do but drop the frame
      ++re.dropped_frames;
      return 0;
    }
  }

  // if we are actually writing the video file, do so
  if (re.writer) {
    auto data = edata.getData();
    re.writer->write((uint8_t *)data.begin(), data.size(), idx.getTimestampEof() / 1000, false, flags & V4L2_BUF_FLAG_KEYFRAME);
  }

  // put it in log stream as the idx packet
  MessageBuilder bmsg;
  auto evt = bmsg.initEvent(event.getValid());
  evt.setLogMonoTime(event.getLogMonoTime());
  (evt.*(re.encoder_info.set_encode_idx_func))(idx);
  auto new_msg = bmsg.toBytes();
  s->logger.write((uint8_t *)new_msg.begin(), new_msg.size(), true);  // always in qlog?
  return new_msg.size();
}

size_t write_encode_data(LoggerdState *s, Message *msg, struct RemoteEncoder &re) {
  capnp::FlatArrayMessageReader cmsg(kj::ArrayPtr<capnp::word>((capnp::word *)msg->getData(), msg->getSize() / sizeof(capnp::word)));
  auto event = cmsg.getRoot<cereal::Event>();
  return write_encode_data(s, event, re);
}

int handle_encoder_msg(LoggerdState *s, Message *raw_msg, struct RemoteEncoder &re) {
  std::unique_ptr<Message> msg(raw_msg);

  capnp::FlatArrayMessageReader cmsg(kj::ArrayPtr<capnp::word>((capnp::word *)msg->getData(), msg->getSize() / sizeof(capnp::word)));
  auto event = cmsg.getRoot<cereal::Event>();
  auto idx = (event.*(re.encoder_info.get_encode_data_func))().getIdx();

  if (re.prev_encoder_segment_num != idx.getSegmentNum()) {
    re.prev_encoder_segment_num = idx.getSegmentNum();
    ++re.current_segment;
  }

  if (re.current_segment != s->logger.segment()) {
    // encoderd packet has a newer segment, this means encoderd has rolled over
    if (!re.marked_ready_to_rotate) {
      re.writer.reset(nullptr);
      re.marked_ready_to_rotate = true;
      ++s->ready_to_rotate;
      LOGD("rotate %d -> %d ready %d/%d for %s", s->logger.segment(), re.current_segment,
          s->ready_to_rotate.load(), s->max_waiting, re.encoder_info.publish_name);
    }
    // queue up all the new segment messages, they go in after the rotate
    re.q.emplace_back(std::move(msg));
    return 0;
  }

  // loggerd is now on the segment that matches this packet

  int bytes_count = 0;
  if (!re.q.empty()) {
    for (auto &qmsg : re.q) {
      bytes_count += write_encode_data(s, qmsg.get(), re);
    }
    re.q.clear();
  }
  bytes_count += write_encode_data(s, event, re);
  return bytes_count;
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

  std::unique_ptr<Context> ctx(Context::create());
  std::unique_ptr<Poller> poller(Poller::create());

  LoggerdState s;
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

    for (const auto &cam : cameras_logged) {
      for (const auto &encoder_info : cam.encoder_infos) {
        if (encoder_info.publish_name == it.name) {
          s.remote_encoders[sock] = std::make_unique<RemoteEncoder>(encoder_info);
          break;
        }
      }
    }
  }

  // init logger
  logger_rotate(&s);
  Params().put("CurrentRoute", s.logger.routeName());

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
          bytes_count += handle_encoder_msg(&s, msg, *(s.remote_encoders[sock]));
        } else {
          s.logger.write((uint8_t *)msg->getData(), msg->getSize(), in_qlog);
          bytes_count += msg->getSize();
          delete msg;
        }

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
