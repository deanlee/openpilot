#pragma once

#include <QPointF>
#include <QTransform>

#include "cereal/messaging/messaging.h"
#include "common/mat.h"

constexpr mat3 DEFAULT_CALIBRATION = {{0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0}};
constexpr mat3 FCAM_INTRINSIC_MATRIX = (mat3){{2648.0, 0.0, 1928.0 / 2,
                                               0.0, 2648.0, 1208.0 / 2,
                                               0.0, 0.0, 1.0}};
// tici ecam focal probably wrong? magnification is not consistent across frame
// Need to retrain model before this can be changed
constexpr mat3 ECAM_INTRINSIC_MATRIX = (mat3){{567.0, 0.0, 1928.0 / 2,
                                               0.0, 567.0, 1208.0 / 2,
                                               0.0, 0.0, 1.0}};

class Calibration {
 public:
  Calibration();
  void setSize(int w, int h);
  void setWideFrame(bool wide) { wide_cam = wide; }
  void update(cereal::LiveCalibrationData::Reader live_calib);
  bool calib_frame_to_full_frame(float in_x, float in_y, float in_z, QPointF *out) const;

 protected:
  bool calibration_valid = false;
  bool calibration_wide_valid = false;
  bool wide_cam = false;
  mat3 view_from_calib = DEFAULT_CALIBRATION;
  mat3 view_from_wide_calib = DEFAULT_CALIBRATION;
  QTransform car_space_transform;
  int fb_w = 0;
  int fb_h = 0;

  float x_offset = 0;
  float y_offset = 0;
  float zoom = 1.0;
  mat3 calibration = DEFAULT_CALIBRATION;
  mat3 intrinsic_matrix = FCAM_INTRINSIC_MATRIX;
};
