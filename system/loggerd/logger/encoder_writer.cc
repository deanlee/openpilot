#include "system/loggerd/logger/encoder_writer.h"

int handle_encoder_msg(LoggerdState *s, Message *msg, std::string &name, struct RemoteEncoder &re, const EncoderInfo &encoder_info) {
  int bytes_count = 0;

  // extract the message
  capnp::FlatArrayMessageReader cmsg(kj::ArrayPtr<capnp::word>((capnp::word *)msg->getData(), msg->getSize() / sizeof(capnp::word)));
  auto event = cmsg.getRoot<cereal::Event>();
  auto edata = (event.*(encoder_info.get_encode_data_func))();
  auto idx = edata.getIdx();
  auto flags = idx.getFlags();

  // encoderd can have started long before loggerd
  if (!re.seen_first_packet) {
    re.seen_first_packet = true;
    re.encoderd_segment_offset = idx.getSegmentNum();
    LOGD("%s: has encoderd offset %d", name.c_str(), re.encoderd_segment_offset);
  }
  int offset_segment_num = idx.getSegmentNum() - re.encoderd_segment_offset;

  if (offset_segment_num == s->rotate_segment) {
    // loggerd is now on the segment that matches this packet

    // if this is a new segment, we close any possible old segments, move to the new, and process any queued packets
    if (re.current_segment != s->rotate_segment) {
      if (re.recording) {
        re.writer.reset();
        re.recording = false;
      }
      re.current_segment = s->rotate_segment;
      re.marked_ready_to_rotate = false;
      // we are in this segment now, process any queued messages before this one
      if (!re.q.empty()) {
        for (auto &qmsg: re.q) {
          bytes_count += handle_encoder_msg(s, qmsg, name, re, encoder_info);
        }
        re.q.clear();
      }
    }

    // if we aren't recording yet, try to start, since we are in the correct segment
    if (!re.recording) {
      if (flags & V4L2_BUF_FLAG_KEYFRAME) {
        // only create on iframe
        if (re.dropped_frames) {
          // this should only happen for the first segment, maybe
          LOGW("%s: dropped %d non iframe packets before init", name.c_str(), re.dropped_frames);
          re.dropped_frames = 0;
        }
        // if we aren't actually recording, don't create the writer
        if (encoder_info.record) {
          re.writer.reset(new VideoWriter(s->segment_path,
            encoder_info.filename, idx.getType() != cereal::EncodeIndex::Type::FULL_H_E_V_C,
            encoder_info.frame_width, encoder_info.frame_height, encoder_info.fps, idx.getType()));
          // write the header
          auto header = edata.getHeader();
          re.writer->write((uint8_t *)header.begin(), header.size(), idx.getTimestampEof()/1000, true, false);
        }
        re.recording = true;
      } else {
        // this is a sad case when we aren't recording, but don't have an iframe
        // nothing we can do but drop the frame
        delete msg;
        ++re.dropped_frames;
        return bytes_count;
      }
    }

    // we have to be recording if we are here
    assert(re.recording);

    // if we are actually writing the video file, do so
    if (re.writer) {
      auto data = edata.getData();
      re.writer->write((uint8_t *)data.begin(), data.size(), idx.getTimestampEof()/1000, false, flags & V4L2_BUF_FLAG_KEYFRAME);
    }

    // put it in log stream as the idx packet
    MessageBuilder bmsg;
    auto evt = bmsg.initEvent(event.getValid());
    evt.setLogMonoTime(event.getLogMonoTime());
    (evt.*(encoder_info.set_encode_idx_func))(idx);
    auto new_msg = bmsg.toBytes();
    logger_log(&s->logger, (uint8_t *)new_msg.begin(), new_msg.size(), true);   // always in qlog?
    bytes_count += new_msg.size();

    // free the message, we used it
    delete msg;
  } else if (offset_segment_num > s->rotate_segment) {
    // encoderd packet has a newer segment, this means encoderd has rolled over
    if (!re.marked_ready_to_rotate) {
      re.marked_ready_to_rotate = true;
      ++s->ready_to_rotate;
      LOGD("rotate %d -> %d ready %d/%d for %s",
        s->rotate_segment.load(), offset_segment_num,
        s->ready_to_rotate.load(), s->max_waiting, name.c_str());
    }
    // queue up all the new segment messages, they go in after the rotate
    re.q.push_back(msg);
  } else {
    LOGE("%s: encoderd packet has a older segment!!! idx.getSegmentNum():%d s->rotate_segment:%d re.encoderd_segment_offset:%d",
      name.c_str(), idx.getSegmentNum(), s->rotate_segment.load(), re.encoderd_segment_offset);
    // free the message, it's useless. this should never happen
    // actually, this can happen if you restart encoderd
    re.encoderd_segment_offset = -s->rotate_segment.load();
    delete msg;
  }

  return bytes_count;
}
