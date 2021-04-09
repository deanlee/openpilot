#pragma once

#include <stdbool.h>

#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif

#include "camera_common.h"

#define FRAME_BUF_COUNT 16

typedef struct CameraState {
  CameraInfo ci;
  int camera_num;
  int fps;
  float digital_gain;
  CameraBuf buf;
} CameraState;


class MultiCameraState : public CamerasBase {
public:
  MultiCameraState() : CamerasBase() {}
  void init() override;
  void run() override;
  void close() override;

  CameraState road_cam;
  CameraState driver_cam;
};
