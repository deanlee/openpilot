#include "tools/replay/framereader.h"
#include "tools/replay/util.h"

#include <cassert>
#include "libyuv.h"

#include "cereal/visionipc/visionbuf.h"

#ifdef __APPLE__
#define HW_DEVICE_TYPE AV_HWDEVICE_TYPE_VIDEOTOOLBOX
#define HW_PIX_FMT AV_PIX_FMT_VIDEOTOOLBOX
#else
#define HW_DEVICE_TYPE AV_HWDEVICE_TYPE_CUDA
#define HW_PIX_FMT AV_PIX_FMT_CUDA
#endif

namespace {

enum AVPixelFormat get_hw_format(AVCodecContext *ctx, const enum AVPixelFormat *pix_fmts) {
  enum AVPixelFormat *hw_pix_fmt = reinterpret_cast<enum AVPixelFormat *>(ctx->opaque);
  for (const enum AVPixelFormat *p = pix_fmts; *p != -1; p++) {
    if (*p == *hw_pix_fmt) return *p;
  }
  rWarning("Please run replay with the --no-hw-decoder flag!");
  // fallback to YUV420p
  *hw_pix_fmt = AV_PIX_FMT_NONE;
  return AV_PIX_FMT_YUV420P;
}

}  // namespace

FrameReader::FrameReader() {
  av_log_set_level(AV_LOG_QUIET);
}

FrameReader::~FrameReader() {
  for (AVPacket *pkt : packets) {
    av_packet_free(&pkt);
  }

  if (decoder_ctx) avcodec_free_context(&decoder_ctx);
  if (hw_device_ctx) av_buffer_unref(&hw_device_ctx);

}

bool FrameReader::load(const std::string &url, bool no_hw_decoder, std::atomic<bool> *abort, bool local_cache, int chunk_size, int retries) {
  FileReader f(local_cache, chunk_size, retries);
  data1 = f.read(url, abort);
  if (data1.empty()) return false;

  return load((std::byte *)data1.data(), data1.size(), no_hw_decoder, abort);
}

bool FrameReader::load(const std::byte *data, int size, bool no_hw_decoder, std::atomic<bool> *abort) {
  const AVCodec *decoder = avcodec_find_decoder(AV_CODEC_ID_HEVC);
  if (!decoder) return false;

  decoder_ctx = avcodec_alloc_context3(decoder);

  width = 1928;//(decoder_ctx->width + 3) & ~3;
  height = 1208;//decoder_ctx->height;
  visionbuf_compute_aligned_width_and_height(width, height, &aligned_width, &aligned_height);

  // if (has_hw_decoder && !no_hw_decoder) {
  //   if (!initHardwareDecoder(HW_DEVICE_TYPE)) {
  //     rWarning("No device with hardware decoder found. fallback to CPU decoding.");
  //   }
  // }
  
  int ret = avcodec_open2(decoder_ctx, decoder, nullptr);
  if (ret < 0) return false;

  valid_ = true;
  auto parser = av_parser_init(decoder->id);
  packets.reserve(60 * 20);  // 20fps, one minute
  while (size > 0 && !(abort && *abort)) {
    AVPacket *pkt = av_packet_alloc();
    ret = av_parser_parse2(parser, decoder_ctx, &pkt->data, &pkt->size,
                           (const uint8_t*) data, size, AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
    if (ret < 0) {
      valid_ = false;;
      break;
    }
    data += ret;
    size -= ret;
    packets.push_back(pkt);
    // printf("%d\n", size);
    // some stream seems to contain no keyframes
    key_frames_count_ += pkt->flags & AV_PKT_FLAG_KEY;
  }
  printf("size is %zu %d\n", packets.size(), valid_);
  // av_parser_close(parser);
  valid_ = valid_ && !packets.empty();
  return valid_;
}

bool FrameReader::initHardwareDecoder(AVHWDeviceType hw_device_type) {
  for (int i = 0;; i++) {
    const AVCodecHWConfig *config = avcodec_get_hw_config(decoder_ctx->codec, i);
    if (!config) {
      rWarning("decoder %s does not support hw device type %s.", decoder_ctx->codec->name,
               av_hwdevice_get_type_name(hw_device_type));
      return false;
    }
    if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX && config->device_type == hw_device_type) {
      hw_pix_fmt = config->pix_fmt;
      break;
    }
  }

  int ret = av_hwdevice_ctx_create(&hw_device_ctx, hw_device_type, nullptr, nullptr, 0);
  if (ret < 0) {
    hw_pix_fmt = AV_PIX_FMT_NONE;
    has_hw_decoder = false;
    rWarning("Failed to create specified HW device %d.", ret);
    return false;
  }

  decoder_ctx->hw_device_ctx = av_buffer_ref(hw_device_ctx);
  decoder_ctx->opaque = &hw_pix_fmt;
  decoder_ctx->get_format = get_hw_format;
  return true;
}

bool FrameReader::get(int idx, uint8_t *yuv) {
  assert(yuv != nullptr);
  if (!valid_ || idx < 0 || idx >= packets.size()) {
    return false;
  }
  return decode(idx, yuv);
}

bool FrameReader::decode(int idx, uint8_t *yuv) {
  printf("decode %d\n", key_frames_count_);
  int from_idx = idx;
  if (idx != prev_idx + 1 && key_frames_count_ > 1) {
    // seeking to the nearest key frame
    for (int i = idx; i >= 0; --i) {
      if (packets[i]->flags & AV_PKT_FLAG_KEY) {
        from_idx = i;
        break;
      }
    }
  }
  prev_idx = idx;

  for (int i = from_idx; i <= idx; ++i) {
    AVFrame *f = decodeFrame(packets[i]);
    if (f && i == idx) {
      return copyBuffers(f, yuv);
    }
  }
  return false;
}

AVFrame *FrameReader::decodeFrame(AVPacket *pkt) {
  printf("here1\n");
  int ret = avcodec_send_packet(decoder_ctx, pkt);
  if (ret < 0) {
    rError("Error sending a packet for decoding: %d", ret);
    return nullptr;
  }
printf("here2\n");
  av_frame_.reset(av_frame_alloc());
  ret = avcodec_receive_frame(decoder_ctx, av_frame_.get());
  if (ret != 0) {
    rError("avcodec_receive_frame error: %d", ret);
    return nullptr;
  }
  printf("here3\n");

  if (av_frame_->format == hw_pix_fmt) {
    hw_frame.reset(av_frame_alloc());
    if ((ret = av_hwframe_transfer_data(hw_frame.get(), av_frame_.get(), 0)) < 0) {
      rError("error transferring the data from GPU to CPU");
      return nullptr;
    }
    return hw_frame.get();
  } else {
    return av_frame_.get();
  }
}

bool FrameReader::copyBuffers(AVFrame *f, uint8_t *yuv) {
  assert(f != nullptr && yuv != nullptr);
  uint8_t *y = yuv;
  uint8_t *uv = y + width * height;
  if (hw_pix_fmt == HW_PIX_FMT) {
    for (int i = 0; i < height/2; i++) {
      memcpy(y + (i*2 + 0)*width, f->data[0] + (i*2 + 0)*f->linesize[0], width);
      memcpy(y + (i*2 + 1)*width, f->data[0] + (i*2 + 1)*f->linesize[0], width);
      memcpy(uv + i*width, f->data[1] + i*f->linesize[1], width);
    }
  } else {
    libyuv::I420ToNV12(f->data[0], f->linesize[0],
                       f->data[1], f->linesize[1],
                       f->data[2], f->linesize[2],
                       y, width,
                       uv, width,
                       width, height);
  }
  return true;
}
