#pragma once


#include "msgq/visionipc/visionipc_client.h"
#include <memory>

const int FRAME_BUFFER_SIZE = 5;
// static_assert(FRAME_BUFFER_SIZE <= YUV_BUFFER_COUNT);

class CameraView {

public:
  CameraView(const std::string &stream_name, VisionStreamType stream_type, bool zoom);
  ~CameraView();
  void setStreamType(VisionStreamType type) { requested_stream_type = type; }
  VisionStreamType getStreamType() { return active_stream_type; }
   void draw();

protected:
  void connect();

  bool zoomed_view;
  // mat4 frame_mat = {};
  // QColor bg = QColor("#000000");

  std::string stream_name;
  int stream_width = 0;
  int stream_height = 0;
  int stream_stride = 0;
  VisionStreamType active_stream_type;
  VisionStreamType requested_stream_type;
  std::set<VisionStreamType> available_streams;
  std::unique_ptr<VisionIpcClient> vipc_client;

private:
  struct Private;
  Private *priv = nullptr;
};
