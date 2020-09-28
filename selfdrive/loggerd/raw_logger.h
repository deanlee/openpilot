#pragma once

#include <cstdio>
#include <cstdlib>

#include <string>
#include <vector>

extern "C" {
#include <libavutil/imgutils.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#include "frame_logger.h"

class RawLogger : public FrameLogger {
public:
  RawLogger(const std::string &filename, int awidth, int aheight, int afps);
  virtual ~RawLogger();

  bool ProcessFrame(uint64_t ts, const uint8_t *y_ptr, const uint8_t *u_ptr, const uint8_t *v_ptr, int width, int height, VIPCBufExtra *extra);
  void Open(const std::string &path);
  void Close();

private:
  AVCodec *codec = NULL;
  AVCodecContext *codec_ctx = NULL;

  AVStream *stream = NULL;
  AVFormatContext *format_ctx = NULL;

  AVFrame *frame = NULL;
};
