#include "selfdrive/ui/qt/onroad/alerts.h"

#include <map>

#include "selfdrive/ui/qt/util.h"
#include "selfdrive/ui/qt/onroad/raylib_helpers.h"

void OnroadAlerts::updateState(const UIState &s) {
  Alert a = getAlert(*(s.sm), s.scene.started_frame);
  if (!alert.equal(a)) {
    alert = a;
  }
}

void OnroadAlerts::clear() {
  alert = {};
}

OnroadAlerts::Alert OnroadAlerts::getAlert(const SubMaster &sm, uint64_t started_frame) {
  const cereal::ControlsState::Reader &cs = sm["controlsState"].getControlsState();
  const uint64_t controls_frame = sm.rcv_frame("controlsState");

  Alert a = {};
  if (controls_frame >= started_frame) {  // Don't get old alert.
    a = {cs.getAlertText1().cStr(), cs.getAlertText2().cStr(),
         cs.getAlertType().cStr(), cs.getAlertSize(), cs.getAlertStatus()};
  }

  if (!sm.updated("controlsState") && (sm.frame - started_frame) > 5 * UI_FREQ) {
    const int CONTROLS_TIMEOUT = 5;
    const int controls_missing = (nanos_since_boot() - sm.rcv_time("controlsState")) / 1e9;

    // Handle controls timeout
    if (controls_frame < started_frame) {
      // car is started, but controlsState hasn't been seen at all
      a = {"openpilot Unavailable", "Waiting for controls to start",
           "controlsWaiting", cereal::ControlsState::AlertSize::MID,
           cereal::ControlsState::AlertStatus::NORMAL};
    } else if (controls_missing > CONTROLS_TIMEOUT && !Hardware::PC()) {
      // car is started, but controls is lagging or died
      if (cs.getEnabled() && (controls_missing - CONTROLS_TIMEOUT) < 10) {
        a = {"TAKE CONTROL IMMEDIATELY", "Controls Unresponsive",
             "controlsUnresponsive", cereal::ControlsState::AlertSize::FULL,
             cereal::ControlsState::AlertStatus::CRITICAL};
      } else {
        a = {"Controls Unresponsive", "Reboot Device",
             "controlsUnresponsivePermanent", cereal::ControlsState::AlertSize::MID,
             cereal::ControlsState::AlertStatus::NORMAL};
      }
    }
  }
  // return {"openpilot Unavailable", "Waiting for controls to start",
  //          "controlsWaiting", cereal::ControlsState::AlertSize::MID,
  //          cereal::ControlsState::AlertStatus::NORMAL};
  return a;
}

void OnroadAlerts::draw() {
  static std::map<cereal::ControlsState::AlertSize, const int> alert_heights = {
      {cereal::ControlsState::AlertSize::SMALL, 271},
      {cereal::ControlsState::AlertSize::MID, 420},
      {cereal::ControlsState::AlertSize::FULL, GetScreenHeight()},
  };

  static const std::map<cereal::ControlsState::AlertStatus, Color> alert_colors = {
      {cereal::ControlsState::AlertStatus::NORMAL, Color{0x15, 0x15, 0x15, 0xf1}},
      {cereal::ControlsState::AlertStatus::USER_PROMPT, Color{0xDA, 0x6F, 0x25, 0xf1}},
      {cereal::ControlsState::AlertStatus::CRITICAL, Color{0xC9, 0x22, 0x31, 0xf1}},
  };

  if (alert.size == cereal::ControlsState::AlertSize::NONE) return;

  float h = alert_heights[alert.size];

  float margin = 40;
  float radius = 30;
  if (alert.size == cereal::ControlsState::AlertSize::FULL) {
    margin = 0;
    radius = 0;
  }

  Rectangle r = {margin, GetScreenHeight() - h - margin, GetScreenWidth() - margin * 2, h - margin * 2};
  DrawRectangle(r.x, r.y, r.width, r.height, alert_colors.at(alert.status));
  DrawRectangleGradientV(r.x, r.y, r.width, r.height, {0, 0, 0, 12}, {0, 0, 0, 90});

  if (alert.size == cereal::ControlsState::AlertSize::SMALL) {
    drawCenteredText(r, alert.text1, 74, WHITE);

  } else if (alert.size == cereal::ControlsState::AlertSize::MID) {
    drawCenteredText(Rectangle{r.x, r.y - 125, r.width, 150}, alert.text1, 88, WHITE);
    drawCenteredText(Rectangle{r.x, r.y + 21, r.width, 90}, alert.text2, 66, WHITE);
  } else if (alert.size == cereal::ControlsState::AlertSize::FULL) {
    bool l = alert.text1.length() > 15;
    float font_size = l ? 132 : 177;
    drawCenteredText(Rectangle{r.x, r.y + (l ? 240 : 270), r.width, 600}, alert.text1, font_size, WHITE);
    drawCenteredText(Rectangle{r.x, r.height - (l ? 361 : 420), r.width, 300}, alert.text1, 88, WHITE);
  }
}
