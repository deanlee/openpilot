#include "selfdrive/ui/qt/onroad/calibration.h"
#include "common/transformations/orientation.hpp"

Calibration::Calibration() {

}

void Calibration::update(cereal::LiveCalibrationData::Reader live_calib) {
  auto rpy_list = live_calib.getRpyCalib();
  auto wfde_list = live_calib.getWideFromDeviceEuler();
  Eigen::Vector3d rpy;
  Eigen::Vector3d wfde;
  if (rpy_list.size() == 3) rpy << rpy_list[0], rpy_list[1], rpy_list[2];
  if (wfde_list.size() == 3) wfde << wfde_list[0], wfde_list[1], wfde_list[2];
  Eigen::Matrix3d device_from_calib = euler2rot(rpy);
  Eigen::Matrix3d wide_from_device = euler2rot(wfde);
  Eigen::Matrix3d view_from_device;
  view_from_device << 0, 1, 0,
                      0, 0, 1,
                      1, 0, 0;
  Eigen::Matrix3d calib = view_from_device * device_from_calib;
  Eigen::Matrix3d wide_calib = view_from_device * wide_from_device * device_from_calib;
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      view_from_calib.v[i*3 + j] = calib(i, j);
      view_from_wide_calib.v[i*3 + j] = wide_calib(i, j);
    }
  }
  calibration_valid = live_calib.getCalStatus() == cereal::LiveCalibrationData::Status::CALIBRATED;
  calibration_wide_valid = wfde_list.size() == 3;
}

// Projects a point in car to space to the corresponding point in full frame
// image space.
bool Calibration::calib_frame_to_full_frame(float in_x, float in_y, float in_z, QPointF *out) const {
  const vec3 pt = (vec3){{in_x, in_y, in_z}};
  const vec3 Ep = matvecmul3(wide_cam ? view_from_wide_calib : view_from_calib, pt);
  const vec3 KEp = matvecmul3(wide_cam ? ECAM_INTRINSIC_MATRIX : FCAM_INTRINSIC_MATRIX, Ep);

  // Project.
  QPointF point = car_space_transform.map(QPointF{KEp.v[0] / KEp.v[2], KEp.v[1] / KEp.v[2]});
  if (clip_region.contains(point)) {
    *out = point;
    return true;
  }
  return false;
}

void Calibration::setSize(int w, int h) {
  fb_w = w;
  fb_h = h;

  // Apply transformation such that video pixel coordinates match video
  // 1) Put (0, 0) in the middle of the video
  // 2) Apply same scaling as video
  // 3) Put (0, 0) in top left corner of video
  car_space_transform.reset();
  car_space_transform.translate(w / 2 - x_offset, h / 2 - y_offset)
      .scale(zoom, zoom)
      .translate(-intrinsic_matrix.v[2], -intrinsic_matrix.v[5]);
}
