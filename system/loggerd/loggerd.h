#pragma once

#include "system/loggerd/encoder_config.h"
#include "system/loggerd/logger.h"
#include "system/loggerd/video_writer.h"

struct LoggerdState {
  LoggerState logger;
  std::atomic<double> last_camera_seen_tms{0.0};
  std::atomic<int> ready_to_rotate{0};  // count of encoders ready to rotate
  int max_waiting = 0;
  double last_rotate_tms = 0.;  // last rotate time in ms
};

class RemoteEncoder {
 public:
  std::unique_ptr<VideoWriter> writer;
  int encoderd_segment_offset;
  int current_segment = -1;
  std::vector<Message *> q;
  int dropped_frames = 0;
  bool recording = false;
  bool marked_ready_to_rotate = false;
  bool seen_first_packet = false;

  bool syncSegment(LoggerdState *s, const std::string &name, int encoder_segment_num, int log_segment_num) {
    if (!seen_first_packet) {
      seen_first_packet = true;
      encoderd_segment_offset = encoder_segment_num - log_segment_num;
      LOGD("%s: has encoderd offset %d", name.c_str(), encoderd_segment_offset);
    }
    int offset_segment_num = encoder_segment_num - encoderd_segment_offset;
    printf("offset %d encoderd_segment_offset: %d, log_segment_num:%d\n", offset_segment_num, encoderd_segment_offset, log_segment_num);
    if (offset_segment_num == log_segment_num) {
      // loggerd is now on the segment that matches this packet

      // if this is a new segment, we close any possible old segments, move to the new, and process any queued packets
      if (current_segment != log_segment_num) {
        if (recording) {
          writer.reset();
          recording = false;
        }
        current_segment = log_segment_num;
        marked_ready_to_rotate = false;
      }
      return true;
    }

    if (offset_segment_num > log_segment_num) {
      // encoderd packet has a newer segment, this means encoderd has rolled over
      if (!marked_ready_to_rotate) {
        marked_ready_to_rotate = true;
        ++s->ready_to_rotate;
        LOGD("rotate %d -> %d ready %d/%d for %s",
             log_segment_num, offset_segment_num,
             s->ready_to_rotate.load(), s->max_waiting, name.c_str());
      }
    } else {
      LOGE("%s: encoderd packet has a older segment!!! idx.getSegmentNum():%d s->logger.segment():%d re.encoderd_segment_offset:%d",
           name.c_str(), encoder_segment_num, log_segment_num, encoderd_segment_offset);
      // free the message, it's useless. this should never happen
      // actually, this can happen if you restart encoderd
      encoderd_segment_offset = encoder_segment_num - log_segment_num;
    }
    return false;
  }
};
