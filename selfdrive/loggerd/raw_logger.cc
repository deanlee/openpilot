#pragma clang diagnostic ignored "-Wdeprecated-declarations"

#include "selfdrive/loggerd/raw_logger.h"

#include <fcntl.h>
#include <unistd.h>

#include <cassert>
#include <cstdio>
#include <cstdlib>

#define __STDC_CONSTANT_MACROS

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
}

#include "selfdrive/common/swaglog.h"
#include "selfdrive/common/util.h"

RawLogger::RawLogger(const char* filename, int width, int height, int fps,
                     int bitrate, bool h265, bool downscale, bool write)
  : filename(filename), fps(fps) {

  // TODO: respect write arg

  av_register_all();
  codec = avcodec_find_encoder(AV_CODEC_ID_HEVC);
  assert(codec);

  codec_ctx = avcodec_alloc_context3(codec);
  assert(codec_ctx);
  codec_ctx->width = width;
  codec_ctx->height = height;
  codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
  codec_ctx->bit_rate = bitrate;

  // codec_ctx->time_base.den = 25;
  // codec_ctx->time_base.num = 1;
  codec_ctx->framerate.den = 1;
  codec_ctx->framerate.num = 25;
  codec_ctx->qmin = 10;
  codec_ctx->qmax = 51;

  codec_ctx->gop_size = 250;
  codec_ctx->max_b_frames = 3;

  // codec_ctx->thread_count = 2;

  // codec_ctx->gop_size = 10;
  codec_ctx->time_base = (AVRational){ 1, fps };

  int err = avcodec_open2(codec_ctx, codec, NULL);
  assert(err >= 0);

  frame = av_frame_alloc();
  assert(frame);
  frame->format = codec_ctx->pix_fmt;
  frame->width = width;
  frame->height = height;
  frame->linesize[0] = width;
  frame->linesize[1] = width/2;
  frame->linesize[2] = width/2;
}

RawLogger::~RawLogger() {
  av_frame_free(&frame);
  avcodec_close(codec_ctx);
  av_free(codec_ctx);
}

void RawLogger::encoder_open(const char* path) {
  vid_path = util::string_format("%s/%s", path, filename);

  // create camera lock file
  lock_path = util::string_format("%s/%s.lock", path, filename);

  LOG("open %s\n", lock_path.c_str());

  int lock_fd = HANDLE_EINTR(open(lock_path.c_str(), O_RDWR | O_CREAT, 0664));
  assert(lock_fd >= 0);
  close(lock_fd);

  format_ctx = NULL;
  avformat_alloc_output_context2(&format_ctx, NULL, NULL, vid_path.c_str());
  assert(format_ctx);

  stream = avformat_new_stream(format_ctx, codec);
  // AVStream *stream = avformat_new_stream(format_ctx, NULL);
  assert(stream);
  stream->id = 0;
  stream->time_base = (AVRational){ 1, fps };
  // codec_ctx->time_base = stream->time_base;

  int err = avcodec_parameters_from_context(stream->codecpar, codec_ctx);
  assert(err >= 0);

  err = avio_open(&format_ctx->pb, vid_path.c_str(), AVIO_FLAG_WRITE);
  assert(err >= 0);

  err = avformat_write_header(format_ctx, NULL);
  assert(err >= 0);

  is_open = true;
  counter = 0;
}

void RawLogger::encoder_close() {
  if (!is_open) return;

  int err = av_write_trailer(format_ctx);
  assert(err == 0);

  avcodec_close(stream->codec);

  err = avio_closep(&format_ctx->pb);
  assert(err == 0);

  avformat_free_context(format_ctx);
  format_ctx = NULL;

  unlink(lock_path.c_str());
  is_open = false;
}

int RawLogger::encode_frame(const uint8_t *y_ptr, const uint8_t *u_ptr, const uint8_t *v_ptr,
                            int in_width, int in_height, uint64_t ts) {
  AVPacket pkt;
  av_init_packet(&pkt);
  pkt.data = NULL;
  pkt.size = 0;
    // AVRational in_timebase = {1, 1000000};
  // enum AVRounding rnd = static_cast<enum AVRounding>(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX);
  // pkt.pts = pkt.dts = av_rescale_q_rnd(ts, in_timebase, format_ctx->streams[0]->time_base, rnd);
  // pkt.duration = av_rescale_q(50*1000, in_timebase, format_ctx->streams[0]->time_base);

  frame->data[0] = (uint8_t*)y_ptr;
  frame->data[1] = (uint8_t*)u_ptr;
  frame->data[2] = (uint8_t*)v_ptr;
  frame->pts= counter*(codec_ctx->time_base.den)/((codec_ctx->time_base.num)*20);
  // frame->pts = av_rescale_q_rnd(ts, in_timebase, format_ctx->streams[0]->time_base, rnd);

  int ret = counter;

  int got_output = 0;
  int err = avcodec_encode_video2(codec_ctx, &pkt, frame, &got_output);
  if (err) {
    LOGE("encoding error\n");
    ret = -1;
  } else if (got_output) {
    av_packet_rescale_ts(&pkt, codec_ctx->time_base, stream->time_base);
    pkt.stream_index = 0;

    err = av_interleaved_write_frame(format_ctx, &pkt);
    if (err < 0) {
      LOGE("encoder writer error\n");
      ret = -1;
    } else {
      counter++;
    }
  }

  av_packet_unref(&pkt);
  return ret;
}
