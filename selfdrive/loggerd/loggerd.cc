#include "selfdrive/loggerd/loggerd.h"
#include "selfdrive/loggerd/video_writer.h"

ExitHandler do_exit;

struct RemoteEncoder;
struct LoggerdState {
  LoggerState logger = {};
  char segment_path[4096];
  std::atomic<int> rotate_segment;
  std::atomic<double> last_camera_seen_tms;
  std::atomic<int> ready_to_rotate;  // count of encoders ready to rotate
  int max_waiting = 0;
  double last_rotate_tms = 0.;      // last rotate time in ms
  std::unordered_map<std::string, std::unique_ptr<RemoteEncoder>> encoders;
};

void logger_rotate(LoggerdState *s) {
  int segment = -1;
  int err = logger_next(&s->logger, LOG_ROOT.c_str(), s->segment_path, sizeof(s->segment_path), &segment);
  assert(err == 0);
  s->rotate_segment = segment;
  s->ready_to_rotate = 0;
  s->last_rotate_tms = millis_since_boot();
  LOGW((s->logger.part == 0) ? "logging to %s" : "rotated to %s", s->segment_path);
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

struct RemoteEncoder {
  RemoteEncoder(LoggerdState *s, const std::string &name);
  int32_t handlePacket(Message *raw_msg);
  int32_t write(cereal::Event::Reader event);

  LoggerdState *s;
  const std::string name;
  LogCameraInfo cam_info = {};
  cereal::EncodeData::Reader (cereal::Event::Reader::*getEncodeData)() const;
  void (cereal::Event::Builder::*setEncodeIdx)(::cereal::EncodeIndex::Reader value);

  std::unique_ptr<VideoWriter> writer;
  int encoderd_segment_offset = -1;
  int current_segment = -1;
  std::vector<std::unique_ptr<Message>> q;
  int dropped_frames = 0;
};

RemoteEncoder::RemoteEncoder(LoggerdState *s, const std::string &name) : s(s), name(name) {
  if (name == "roadEncodeData") {
    cam_info = cameras_logged[0];
    getEncodeData = &cereal::Event::Reader::getRoadEncodeData;
    setEncodeIdx = &cereal::Event::Builder::setRoadEncodeIdx;
  } else if (name == "driverEncodeData") {
    cam_info = cameras_logged[1];
    getEncodeData = &cereal::Event::Reader::getDriverEncodeData;
    setEncodeIdx = &cereal::Event::Builder::setDriverEncodeIdx;
  } else if (name == "wideRoadEncodeData") {
    cam_info = cameras_logged[2];
    getEncodeData = &cereal::Event::Reader::getWideRoadEncodeData;
    setEncodeIdx = &cereal::Event::Builder::setWideRoadEncodeIdx;
  } else {
    cam_info = qcam_info;
    getEncodeData = &cereal::Event::Reader::getQRoadEncodeData;
    setEncodeIdx = &cereal::Event::Builder::setQRoadEncodeIdx;
  }
}

int32_t RemoteEncoder::write(cereal::Event::Reader event) {
  // if this is a new segment, we close any possible old segments, move to the new, and process any queued packets
  if (current_segment != s->rotate_segment) {
    writer.reset();
    current_segment = s->rotate_segment;
  }

  const auto edata = (event.*getEncodeData)();
  const auto idx = edata.getIdx();
  const bool is_key_frame = idx.getFlags() & V4L2_BUF_FLAG_KEYFRAME;

  // if we aren't recording yet, try to start, since we are in the correct segment
  if (!writer && cam_info.record) {
    if (is_key_frame) {
      // only create on iframe
      if (dropped_frames) {
        // this should only happen for the first segment, maybe
        LOGW("%s: dropped %d non iframe packets before init", name.c_str(), dropped_frames);
        dropped_frames = 0;
      }
      // if we aren't actually recording, don't create the writer
      writer.reset(new VideoWriter(s->segment_path,
        cam_info.filename, idx.getType() != cereal::EncodeIndex::Type::FULL_H_E_V_C,
        cam_info.frame_width, cam_info.frame_height, cam_info.fps, idx.getType()));
      // write the header
      auto header = edata.getHeader();
      writer->write((uint8_t *)header.begin(), header.size(), idx.getTimestampEof()/1000, true, false);
    } else {
      // this is a sad case when we aren't recording, but don't have an iframe
      // nothing we can do but drop the frame
      ++dropped_frames;
      return 0;
    }
  }

  // if we are actually writing the video file, do so
  if (writer) {
    auto data = edata.getData();
    writer->write((uint8_t *)data.begin(), data.size(), idx.getTimestampEof()/1000, false, is_key_frame);
  }

  // put it in log stream as the idx packet
  MessageBuilder msg;
  auto evt = msg.initEvent(event.getValid());
  evt.setLogMonoTime(event.getLogMonoTime());
  (evt.*setEncodeIdx)(idx);
  auto new_msg = msg.toBytes();
  logger_log(&s->logger, (uint8_t *)new_msg.begin(), new_msg.size(), true);   // always in qlog?
  return new_msg.size();
}

int RemoteEncoder::handlePacket(Message *raw_msg) {
  int bytes_count = 0;
  std::unique_ptr<Message> msg(raw_msg);

  // extract the message
  capnp::FlatArrayMessageReader cmsg(kj::ArrayPtr<capnp::word>((capnp::word *)msg->getData(), msg->getSize() / sizeof(capnp::word)));
  auto event = cmsg.getRoot<cereal::Event>();
  const int32_t segment_num = (event.*getEncodeData)().getIdx().getSegmentNum();

  // encoderd can have started long before loggerd
  if (encoderd_segment_offset < 0) {
    encoderd_segment_offset = segment_num;
    LOGD("%s: has encoderd offset %d", name.c_str(), encoderd_segment_offset);
  }
  int offset_segment_num = segment_num - encoderd_segment_offset;
  if (offset_segment_num == s->rotate_segment) {
    // loggerd is now on the segment that matches this packet
    if (!q.empty()) {
      // we are in this segment now, process any queued messages before this one
      for (auto &m: q) {
        capnp::FlatArrayMessageReader reader(kj::ArrayPtr<capnp::word>((capnp::word *)m->getData(), m->getSize() / sizeof(capnp::word)));
        bytes_count += write(reader.getRoot<cereal::Event>());
      }
      q.clear();
    }
    bytes_count += write(event);
  } else if (offset_segment_num > s->rotate_segment) {
    // encoderd packet has a newer segment, this means encoderd has rolled over
    if (q.empty()) {
      ++s->ready_to_rotate;
      LOGD("rotate %d -> %d ready %d/%d for %s",
        s->rotate_segment.load(), offset_segment_num,
        s->ready_to_rotate.load(), s->max_waiting, name.c_str());
    }
    // queue up all the new segment messages, they go in after the rotate
    q.emplace_back(std::move(msg));
  } else {
    LOGE("%s: encoderd packet has a older segment!!! idx.getSegmentNum():%d s->rotate_segment:%d re.encoderd_segment_offset:%d",
      name.c_str(), segment_num, s->rotate_segment.load(), encoderd_segment_offset);
    // free the message, it's useless. this should never happen
    // actually, this can happen if you restart encoderd
    encoderd_segment_offset = -s->rotate_segment.load();
  }

  return bytes_count;
}

int handle_encoder_msg(LoggerdState *s, Message *msg, const std::string &name) {
  auto &re = s->encoders[name];
  if (!re) {
    re.reset(new RemoteEncoder(s, name));
  }
  s->last_camera_seen_tms = millis_since_boot();
  return re->handlePacket(msg);
}


void loggerd_thread() {
  // setup messaging
  typedef struct QlogState {
    std::string name;
    int counter, freq;
    bool encoder;
  } QlogState;
  std::unordered_map<SubSocket*, QlogState> qlog_states;
  std::unordered_map<SubSocket*, struct RemoteEncoder> remote_encoders;

  std::unique_ptr<Context> ctx(Context::create());
  std::unique_ptr<Poller> poller(Poller::create());

  // subscribe to all socks
  for (const auto& it : services) {
    const bool encoder = strcmp(it.name+strlen(it.name)-strlen("EncodeData"), "EncodeData") == 0;
    if (!it.should_log && !encoder) continue;
    LOGD("logging %s (on port %d)", it.name, it.port);

    SubSocket * sock = SubSocket::create(ctx.get(), it.name);
    assert(sock != NULL);
    poller->registerSocket(sock);
    qlog_states[sock] = {
      .name = it.name,
      .counter = 0,
      .freq = it.decimation,
      .encoder = encoder,
    };
  }

  LoggerdState s;
  // init logger
  logger_init(&s.logger, true);
  logger_rotate(&s);
  Params().put("CurrentRoute", s.logger.route_name);

  // init encoders
  s.last_camera_seen_tms = millis_since_boot();
  for (const auto &cam : cameras_logged) {
    s.max_waiting++;
    if (cam.has_qcamera) { s.max_waiting++; }
  }

  uint64_t msg_count = 0, bytes_count = 0;
  double start_ts = millis_since_boot();
  while (!do_exit) {
    // poll for new messages on all sockets
    for (auto sock : poller->poll(1000)) {
      if (do_exit) break;

      // drain socket
      int count = 0;
      QlogState &qs = qlog_states[sock];
      Message *msg = nullptr;
      while (!do_exit && (msg = sock->receive(true))) {
        const bool in_qlog = qs.freq != -1 && (qs.counter++ % qs.freq == 0);

        if (qs.encoder) {
          s.last_camera_seen_tms = millis_since_boot();
          bytes_count += handle_encoder_msg(&s, msg, qs.name);
        } else {
          logger_log(&s.logger, (uint8_t *)msg->getData(), msg->getSize(), in_qlog);
          bytes_count += msg->getSize();
          delete msg;
        }

        rotate_if_needed(&s);

        if ((++msg_count % 1000) == 0) {
          double seconds = (millis_since_boot() - start_ts) / 1000.0;
          LOGD("%lu messages, %.2f msg/sec, %.2f KB/sec", msg_count, msg_count / seconds, bytes_count * 0.001 / seconds);
        }

        count++;
        if (count >= 200) {
          LOGD("large volume of '%s' messages", qs.name.c_str());
          break;
        }
      }
    }
  }

  LOGW("closing logger");
  logger_close(&s.logger, &do_exit);

  if (do_exit.power_failure) {
    LOGE("power failure");
    sync();
    LOGE("sync done");
  }

  // messaging cleanup
  for (auto &[sock, qs] : qlog_states) delete sock;
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
