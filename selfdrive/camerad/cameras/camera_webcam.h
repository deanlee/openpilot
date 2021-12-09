#pragma once

#include "selfdrive/camerad/cameras/camera_common.h"

#define FRAME_BUF_COUNT 16

class CameraState : public CameraStateBase {
public:
  CameraState(CameraType cam_type) : CameraStateBase(cam_type) {}
};

typedef struct MultiCameraState {
  CameraState road_cam{RoadCam};
  CameraState driver_cam{DriverCam};

  SubMaster *sm;
  PubMaster *pm;
} MultiCameraState;
