#pragma once

#include <memory>
#include <string>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

#include "cereal/messaging/messaging.h"
#include "system/loggerd/logger.h"

class VideoWriter {
public:
  VideoWriter(const char *path, const char *filename, bool remuxing, int width, int height, int fps, cereal::EncodeIndex::Type codec);
  void write(uint8_t *data, int len, long long timestamp, bool codecconfig, bool keyframe);
  ~VideoWriter();
private:
  std::string vid_path, lock_path;
  std::unique_ptr<RawFile> file_writer;
  AVCodecContext *codec_ctx;
  AVFormatContext *ofmt_ctx;
  AVStream *out_stream;
  bool remuxing, raw;
};
