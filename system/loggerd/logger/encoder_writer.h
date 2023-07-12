#pragma once

#include "system/loggerd/logger/logger.h"
#include "system/loggerd/logger/video_writer.h""

class RemoteEncoder {
public:
  RemoteEncoder(const EncoderInfo &info) : info(info) {}
  void rotate(const std::string &path);
  size_t handle_msg(LoggerState* logger, Message *msg);
  inline static int ready_to_rotate = 0;  // count of encoders ready to rotate

private:
  int write_encode_data(LoggerState *logger, const cereal::Event::Reader event);

  std::unique_ptr<VideoWriter> writer;
  int remote_encoder_segment = -1;
  int current_encoder_segment = -1;
  std::string segment_path;
  std::vector<Message *> q;
  int dropped_frames = 0;
  bool recording = false;
  bool marked_ready_to_rotate = false;
  EncoderInfo info;
};
