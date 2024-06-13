import os
from functools import wraps
# import cereal.messaging as messaging

# from openpilot.selfdrive.car.car_helpers import get_startup_event
from cereal import car, log
# from openpilot.selfdrive.controls.lib.events import ET
from openpilot.selfdrive.controls.lib.alertmanager import set_offroad_alert
from openpilot.common.realtime import DT_CTRL
from openpilot.common.swaglog import cloudlog
EventName = car.CarEvent.EventName
ButtonType = car.CarState.ButtonEvent.Type
LaneChangeState = log.LaneChangeState
LaneChangeDirection = log.LaneChangeDirection
SafetyModel = car.CarParams.SafetyModel
ThermalStatus = log.DeviceState.ThermalStatus

REPLAY = "REPLAY" in os.environ
SIMULATION = "SIMULATION" in os.environ
TESTING_CLOSET = "TESTING_CLOSET" in os.environ
IGNORED_SAFETY_MODES = (SafetyModel.silent, SafetyModel.noOutput)
IGNORE_PROCESSES = {"loggerd", "encoderd", "statsd"}
CSID_MAP = {"1": EventName.roadCameraError, "2": EventName.wideRoadCameraError, "0": EventName.driverCameraError}

events_cache:dict[str, list[int]] = {}

def cache_function(key):
  def decorator(func):

    @wraps(func)
    def wrapper(self, sm, events, *args, **kwargs):
      if sm.updated[key]:
        self.events_cache[key] = func(self, sm, *args, **kwargs)
      for e in self.events_cache.get(key, []):
        events.add(e)

    return wrapper
  return decorator

class ControlsEvent:
  def __init__(self):
    self.cached_events: dict[str, list[int]] = {}
    self.last_functional_fan_frame = 0
    self.recalibrating_seen = False
    self.not_running_prev = None

  @cache_function('deviceState')
  def update_device_events(self, sm):
    events = []
    # Create events for temperature, disk space, and memory
    if sm['deviceState'].thermalStatus >= ThermalStatus.red:
      events.append(EventName.overheat)
    if sm['deviceState'].freeSpacePercent < 7 and not SIMULATION:
      # under 7% of space free no enable allowed
      events.append(EventName.outOfSpace)
    if sm['deviceState'].memoryUsagePercent > 90 and not SIMULATION:
      events.append(EventName.lowMemory)

    # TODO: enable this once loggerd CPU usage is more reasonable
    # cpus = list(sm['deviceState'].cpuUsagePercent)
    # if max(cpus, default=0) > 95 and not SIMULATION:
    #   events.add(EventName.highCpuUsage)
    return events

  @cache_function('peripheralState')
  def update_peripheral_event(self, sm):
    events = []
    # Alert if fan isn't spinning for 5 seconds
    if sm['peripheralState'].pandaType != log.PandaState.PandaType.unknown:
      if sm['peripheralState'].fanSpeedRpm < 500 and sm['deviceState'].fanSpeedPercentDesired > 50:
        # allow enough time for the fan controller in the panda to recover from stalls
        if (sm.frame - self.last_functional_fan_frame) * DT_CTRL > 15.0:
          events.append(EventName.fanMalfunction)
      else:
        self.last_functional_fan_frame = sm.frame
    return events

  @cache_function('liveCalibration')
  def update_calibration_events(self, sm):
    events = []
    # Handle calibration status
    cal_status = sm['liveCalibration'].calStatus
    if cal_status != log.LiveCalibrationData.Status.calibrated:
      if cal_status == log.LiveCalibrationData.Status.uncalibrated:
        events.append(EventName.calibrationIncomplete)
      elif cal_status == log.LiveCalibrationData.Status.recalibrating:
        if not self.recalibrating_seen:
          set_offroad_alert("Offroad_Recalibration", True)
        self.recalibrating_seen = True
        events.append(EventName.calibrationRecalibrating)
      else:
        events.append(EventName.calibrationInvalid)

    return events

  @cache_function('modelV2')
  def add_lane_change_events(self, sm, CS):
    events = []
    # Handle lane change
    if sm['modelV2'].meta.laneChangeState == LaneChangeState.preLaneChange:
      direction = sm['modelV2'].meta.laneChangeDirection
      if (CS.leftBlindspot and direction == LaneChangeDirection.left) or \
         (CS.rightBlindspot and direction == LaneChangeDirection.right):
        events.append(EventName.laneChangeBlocked)
      else:
        if direction == LaneChangeDirection.left:
          events.append(EventName.preLaneChangeLeft)
        else:
          events.append(EventName.preLaneChangeRight)
    elif sm['modelV2'].meta.laneChangeState in (LaneChangeState.laneChangeStarting,
                                                LaneChangeState.laneChangeFinishing):
      events.append(EventName.laneChange)

    return events

  @cache_function('pandaStates')
  def add_panda_events(self, sm, controls):
    events = []
    for i, pandaState in enumerate(sm['pandaStates']):
      # All pandas must match the list of safetyConfigs, and if outside this list, must be silent or noOutput
      if i < len(controls.CP.safetyConfigs):
        safety_mismatch = pandaState.safetyModel != controls.CP.safetyConfigs[i].safetyModel or \
                          pandaState.safetyParam != controls.CP.safetyConfigs[i].safetyParam or \
                          pandaState.alternativeExperience != controls.CP.alternativeExperience
      else:
        safety_mismatch = pandaState.safetyModel not in IGNORED_SAFETY_MODES

      # safety mismatch allows some time for pandad to set the safety mode and publish it back from panda
      if (safety_mismatch and sm.frame*DT_CTRL > 10.) or pandaState.safetyRxChecksInvalid or controls.mismatch_counter >= 200:
        events.append(EventName.controlsMismatch)

      if log.PandaState.FaultType.relayMalfunction in pandaState.faults:
        events.append(EventName.relayMalfunction)

    return events

  @cache_function('pandaStates')
  def add_manager_state_events(self, sm, controls):
    events = []
    not_running = {p.name for p in sm['managerState'].processes if not p.running and p.shouldBeRunning}
    if sm.recv_frame['managerState'] and (not_running - IGNORE_PROCESSES):
      events.append(EventName.processNotRunning)
      if not_running != self.not_running_prev:
        cloudlog.event("process_not_running", not_running=not_running, error=True)
      self.not_running_prev = not_running
    else:
      if not SIMULATION and not controls.rk.lagging:
        if not sm.all_alive(controls.camera_packets):
          events.append(EventName.cameraMalfunction)
        elif not sm.all_freq_ok(controls.camera_packets):
          events.append(EventName.cameraFrameRate)
    return events

  @cache_function('pandaStates')
  def add_location_event(self, sm):
    events = []
    if not sm['liveLocationKalman'].posenetOK:
      events.append(EventName.posenetInvalid)
    if not sm['liveLocationKalman'].deviceStable:
      events.append(EventName.deviceFalling)
    if not sm['liveLocationKalman'].inputsOK:
      events.append(EventName.locationdTemporaryError)
    if not sm['liveParameters'].valid and not TESTING_CLOSET and (not SIMULATION or REPLAY):
      events.append(EventName.paramsdTemporaryError)
    return events
