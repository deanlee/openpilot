#!/usr/bin/env python3
import pyray as rl
from cereal import messaging
from openpilot.system.ui.lib.application import gui_app
from openpilot.system.ui.widgets.sidebar import Sidebar, SIDEBAR_WIDTH

def main():
  gui_app.init_window("UI")
  sm = messaging.SubMaster(
    [
      "modelV2",
      "controlsState",
      "liveCalibration",
      "radarState",
      "deviceState",
      "pandaStates",
      "carParams",
      "driverMonitoringState",
      "carState",
      "driverStateV2",
      "roadCameraState",
      "wideRoadCameraState",
      "managerState",
      "selfdriveState",
      "longitudinalPlan",
    ]
  )
  sidbar = Sidebar()
  for _ in gui_app.render():
    sm.update(0)
    sidbar.draw(sm, rl.Rectangle(0, 0, SIDEBAR_WIDTH, gui_app.height))

if __name__ == "__main__":
  main()
