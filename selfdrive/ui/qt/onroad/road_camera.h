#pragma once

#include "selfdrive/ui/qt/widgets/cameraview.h"
#include <string>

class RoadCameraView : public CameraView {
  Q_OBJECT

public:
  RoadCameraView(std::string stream_name, VisionStreamType stream_type, QWidget* parent = nullptr);
  mat3 calibration;
}