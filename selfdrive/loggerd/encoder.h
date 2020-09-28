#pragma once

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include <pthread.h>
#include <memory>

#include <OMX_Component.h>
extern "C" {
#include <libavformat/avformat.h>
}
#include "common/cqueue.h"
#include "frame_logger.h"
class EncoderState : public FrameLogger{
  public:
  EncoderState(const std::string &filename, int width, int height, int fps, int bitrate, bool h265, bool downscale);
  virtual ~EncoderState();
  bool dirty;
    
  FILE *of;

  size_t codec_config_len = 0;
  uint8_t *codec_config = nullptr;
  bool wrote_codec_config = false;

  pthread_mutex_t state_lock;
  pthread_cond_t state_cv;
  OMX_STATETYPE state;

  OMX_HANDLETYPE handle;

  int num_in_bufs;
  OMX_BUFFERHEADERTYPE** in_buf_headers;

  int num_out_bufs;
  OMX_BUFFERHEADERTYPE** out_buf_headers;

  Queue free_in;
  Queue done_out;

  void *stream_sock_raw = nullptr;

  AVFormatContext *ofmt_ctx;
  AVCodecContext *codec_ctx;
  AVStream *out_stream;
  bool remuxing;

  void *zmq_ctx = nullptr;

  std::unique_ptr<uint8_t[]> y_ptr2;
  std::unique_ptr<uint8_t[]> u_ptr2; 
  std::unique_ptr<uint8_t[]> v_ptr2;

  bool ProcessFrame(uint64_t ts, const uint8_t *y_ptr, const uint8_t *u_ptr, const uint8_t *v_ptr, int width, int height, VIPCBufExtra *extra);
private:
  virtual void Open(const std::string &path);
  virtual void Close();
  void Destroy();
};
