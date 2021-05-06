#pragma once

#include <cstdlib>

// #include "selfdrive/common/util.h"
#include "selfdrive/hardware/base.h"

class HardwarePc : public HardwareNone {
public:
  static constexpr float MAX_VOLUME = 1.0;
  static constexpr float MIN_VOLUME = 0.5;

  static const int road_cam_focal_len = 910;
  static const int driver_cam_focal_len = 860;
  inline static const int road_cam_size[] = {1164, 874};
  inline static const int driver_cam_size[] = {1152, 864};
  inline static const int screen_size[] = {1920, 1080};

  static std::string get_os_version() { return "openpilot for PC"; }
  static bool PC() { return true; }
};
