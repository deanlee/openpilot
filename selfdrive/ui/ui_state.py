import pyray as rl
from enum import Enum
import time
from cereal import messaging, log
from openpilot.common.params import Params, UnknownKeyName
from openpilot.selfdrive.ui.lib.prime_state import PrimeState
from openpilot.common.filter_simple import FirstOrderFilter


UI_BORDER_SIZE = 30

_BACKLIGHT_DT = 0.05
_BACKLIGHT_TS = 10.0
_BACKLIGHT_OFFROAD = 50.0

class UIStatus(Enum):
  DISENGAGED = "disengaged"
  ENGAGED = "engaged"
  OVERRIDE = "override"


class UIState:
  _instance: 'UIState | None' = None

  def __new__(cls):
    if cls._instance is None:
      cls._instance = super().__new__(cls)
      cls._instance._initialize()
    return cls._instance

  def _initialize(self):
    self.params = Params()
    self.sm = messaging.SubMaster(
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

    self.prime_state = PrimeState()

    self._last_brightness: int = -1
    self._offroad_brightness: float = _BACKLIGHT_OFFROAD
    self._brightness_filter = FirstOrderFilter(
      _BACKLIGHT_OFFROAD,
      _BACKLIGHT_TS,
      _BACKLIGHT_DT
    )

    # UI Status tracking
    self.status: UIStatus = UIStatus.DISENGAGED
    self.started_frame: int = 0
    self._engaged_prev: bool = False
    self._started_prev: bool = False
    self._brightness_update_time: float = 0.0
    self._brightness_update_interval: float = 0.1  # Update every 100ms


    # Core state variables
    self.is_metric: bool = self.params.get_bool("IsMetric")
    self.started: bool = False
    self.ignition: bool = False
    self.panda_type: log.PandaState.PandaType = log.PandaState.PandaType.unknown
    self.personality: log.LongitudinalPersonality = log.LongitudinalPersonality.standard
    self.light_sensor: float = -1.0

    self._update_params()

  @property
  def engaged(self) -> bool:
    return self.started and self.sm["selfdriveState"].enabled

  def is_onroad(self) -> bool:
    return self.started

  def is_offroad(self) -> bool:
    return not self.started

  def update(self) -> None:
    self.sm.update(0)
    self._update_state()
    self._update_status()

  def _update_state(self) -> None:
    # Handle panda states updates
    if self.sm.updated["pandaStates"]:
      panda_states = self.sm["pandaStates"]

      if len(panda_states) > 0:
        # Get panda type from first panda
        self.panda_type = panda_states[0].pandaType
        # Check ignition status across all pandas
        if self.panda_type != log.PandaState.PandaType.unknown:
          self.ignition = any(state.ignitionLine or state.ignitionCan for state in panda_states)
    elif self.sm.frame - self.sm.recv_frame["pandaStates"] > 5 * rl.get_fps():
      self.panda_type = log.PandaState.PandaType.unknown

    # Handle wide road camera state updates
    if self.sm.updated["wideRoadCameraState"]:
      cam_state = self.sm["wideRoadCameraState"]

      # Scale factor based on sensor type
      scale = 6.0 if cam_state.sensor == 'ar0231' else 1.0
      self.light_sensor = max(100.0 - scale * cam_state.exposureValPercent, 0.0)
    elif not self.sm.alive["wideRoadCameraState"] or not self.sm.valid["wideRoadCameraState"]:
      self.light_sensor = -1

    # Update started state
    self.started = self.sm["deviceState"].started and self.ignition

  def _update_status(self) -> None:
    if self.started and self.sm.updated["selfdriveState"]:
      ss = self.sm["selfdriveState"]
      state = ss.state

      if state in (log.SelfdriveState.OpenpilotState.preEnabled, log.SelfdriveState.OpenpilotState.overriding):
        self.status = UIStatus.OVERRIDE
      else:
        self.status = UIStatus.ENGAGED if ss.enabled else UIStatus.DISENGAGED

    # Check for engagement state changes
    if self.engaged != self._engaged_prev:
      self._engaged_prev = self.engaged

    # Handle onroad/offroad transition
    if self.started != self._started_prev or self.sm.frame == 1:
      if self.started:
        self.status = UIStatus.DISENGAGED
        self.started_frame = self.sm.frame

      self._started_prev = self.started

  def _update_params(self) -> None:
    try:
      self.is_metric = self.params.get_bool("IsMetric")
    except UnknownKeyName:
      self.is_metric = False

  def _update_brightness(self) -> None:
    """Update screen brightness based on light sensor and driving state"""
    current_time = time.time()

    # Throttle brightness updates to avoid excessive system calls
    if current_time - self._brightness_update_time < self._brightness_update_interval:
      return

    clipped_brightness = self._offroad_brightness

    # Use light sensor for automatic brightness when driving
    if self.started and self.light_sensor >= 0:
      clipped_brightness = self.light_sensor

      # Apply CIE 1931 lightness curve
      # https://www.photonstophotos.net/GeneralTopics/Exposure/Psychometric_Lightness_and_Gamma.htm
      if clipped_brightness <= 8:
        clipped_brightness = clipped_brightness / 903.3
      else:
        clipped_brightness = ((clipped_brightness + 16.0) / 116.0) ** 3.0

      # Scale back to 10% to 100% range
      clipped_brightness = max(10.0, min(100.0, 100.0 * clipped_brightness))

    # Apply low-pass filter for smooth transitions
    brightness = self._brightness_filter.update(clipped_brightness)

    # Set brightness to 0 if display should be off
    if not self._is_display_awake():
      brightness = 0

    # Only update if brightness changed significantly
    if abs(brightness - self._last_brightness) >= 1:
      self._set_hardware_brightness(brightness)
      self._last_brightness = brightness
      self._brightness_update_time = current_time


# Global instance
ui_state = UIState()
