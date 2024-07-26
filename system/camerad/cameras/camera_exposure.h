#pragma once

#include <mutex>
#include <vector>

#include "common/params.h"
#include "system/camerad/sensors/sensor.h"

class CameraExposure {
public:
  CameraExposure(int camera_num, const SensorInfo *sensor_info, int width, int height, float focal_len);
  void updateFrameMetaData(FrameMetadata &meta_data);
  std::vector<i2c_random_wr_payload> getExposureRegisters(const CameraBuf &buf, int x_skip, int y_skip);

private:
  void update_exposure_score(float desired_ev, int exp_t, int exp_g_idx, float exp_gain);

  std::mutex exp_lock;

  int exposure_time = 5;
  bool dc_gain_enabled = false;
  int dc_gain_weight = 0;
  int gain_idx = 0;
  float analog_gain_frac = 0;

  float cur_ev[3] = {};
  float best_ev_score = 0;
  int new_exp_g = 0;
  int new_exp_t = 0;

  Rect ae_xywh = {};
  float measured_grey_fraction = 0;
  float target_grey_fraction = 0.3;
  float fl_pix = 0;
  Params params;
  const SensorInfo *ci = nullptr;
};
