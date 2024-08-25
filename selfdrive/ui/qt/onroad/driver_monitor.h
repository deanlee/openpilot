#pragma once
#include "selfdrive/ui/ui.h"

class DriverMonitor {
public:
  DriverMonitor();
  ~DriverMonitor();
  void updateState(const UIState &s);
  void draw();

private:
  float driver_pose_vals[3] = {};
  float driver_pose_diff[3] = {};
  float driver_pose_sins[3] = {};
  float driver_pose_coss[3] = {};
  bool dmActive = 0;
  bool is_rhd = 0;
  float dm_fade_state = 0;
};
