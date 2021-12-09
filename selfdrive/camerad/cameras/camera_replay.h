#pragma once

#include "selfdrive/camerad/cameras/camera_common.h"

class FrameReader;

#define FRAME_BUF_COUNT 16

class CameraState : public CameraStateBase {
public:
  CameraState(CameraType cam_type) : CameraStateBase(cam_type) {}
  FrameReader *frame = nullptr;
};

class MultiCameraState {
public:
  CameraState road_cam{RoadCam};
  CameraState driver_cam{DriverCam};

  SubMaster *sm = nullptr;
  PubMaster *pm = nullptr;
};
