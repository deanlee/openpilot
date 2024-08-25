#pragma once

#include <memory>

#include "selfdrive/ui/qt/onroad/alerts.h"
#include "selfdrive/ui/qt/onroad/driver_monitor.h"
#include "selfdrive/ui/qt/widgets/cameraview.h"
#include "selfdrive/ui/ui.h"

class HudView {
 public:
  HudView();
  ~HudView();
  void draw();
  void drawHud();
  void updateState(const UIState &s);
  void drawLead(const cereal::RadarState::LeadData::Reader &lead_data, const QPointF &vd);
  float speed;
  std::string speedUnit;
  float setSpeed = 0;
  bool is_cruise_set = false;
  bool is_metric = false;
  bool hideBottomIcons = false;
  bool v_ego_cluster_seen = false;
  int status = STATUS_DISENGAGED;
  void *wheel_texture;

  OnroadAlerts alerts;
};

class AnnotatedCamera {
 public:
  AnnotatedCamera();
  void updateState(const UIState &s);
  void draw();

 private:
  std::unique_ptr<CameraView> camera_view;
  std::unique_ptr<HudView> hud;
  std::unique_ptr<DriverMonitor> driver_monitor;
};
