import os
from functools import wraps
import cereal.messaging as messaging

from cereal import car, log
from openpilot.selfdrive.controls.lib.events import ET
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
    def wrapper(self, *args, **kwargs):
      if self.cs.sm.updated(key):
        events_cache[key] = func(self, *args, **kwargs)
      return events_cache.get(key, [])

    return wrapper
  return decorator


class ControlsEvents:
  def __init__(self, controls):
    self.cs.cs = controls
    self.distance_traveled = 0
    self.logged_comm_issue = None
    self.last_functional_fan_frame = 0
    self.cruise_mismatch_counter = 0
    self.not_running_prev = None
    self.recalibrating_seen = False

  @cache_function('deviceState')
  def get_device_events(self):
    events = []
      # Create events for temperature, disk space, and memory
    if self.cs.sm['deviceState'].thermalStatus >= ThermalStatus.red:
      events.append(EventName.overheat)
    if self.cs.sm['deviceState'].freeSpacePercent < 7 and not SIMULATION:
      # under 7% of space free no enable allowed
      events.append(EventName.outOfSpace)
    if self.cs.sm['deviceState'].memoryUsagePercent > 90 and not SIMULATION:
      events.append.add(EventName.lowMemory)
    # TODO: enable this once loggerd CPU usage is more reasonable
    #cpus = list(self.cs.sm['deviceState'].cpuUsagePercent)
    #if max(cpus, default=0) > 95 and not SIMULATION:
    #  events.append(EventName.highCpuUsage)
    return events

  @cache_function('peripheralState')
  def get_peripheral_events(self):
    # Alert if fan isn't spinning for 5 seconds
    if self.cs.sm['peripheralState'].pandaType != log.PandaState.PandaType.unknown:
      if self.cs.sm['peripheralState'].fanSpeedRpm < 500 and self.cs.sm['deviceState'].fanSpeedPercentDesired > 50:
        # allow enough time for the fan controller in the panda to recover from stalls
        if (self.cs.sm.frame - self.last_functional_fan_frame) * DT_CTRL > 15.0:
          return [EventName.fanMalfunction]
      else:
        self.last_functional_fan_frame = self.cs.sm.frame
    return []

  @cache_function('liveCalibration')
  def get_calibration_events(self):
    # Handle calibration status
    events = []
    cal_status = self.cs.sm['liveCalibration'].calStatus
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
  def get_lanechange_events(self, CS):
    events = []
    if not SIMULATION or REPLAY:
      if self.cs.sm['modelV2'].frameDropPerc > 20:
        events.append(EventName.modeldLagging)

    # Handle lane change
    if self.cs.sm['modelV2'].meta.laneChangeState == LaneChangeState.preLaneChange:
      direction = self.cs.sm['modelV2'].meta.laneChangeDirection
      if (CS.leftBlindspot and direction == LaneChangeDirection.left) or \
         (CS.rightBlindspot and direction == LaneChangeDirection.right):
        events.append(EventName.laneChangeBlocked)
      else:
        if direction == LaneChangeDirection.left:
          events.append(EventName.preLaneChangeLeft)
        else:
          events.append(EventName.preLaneChangeRight)
    elif self.cs.sm['modelV2'].meta.laneChangeState in (LaneChangeState.laneChangeStarting,
                                                    LaneChangeState.laneChangeFinishing):
      events.append(EventName.laneChange)

    return events

  @cache_function('pandaStates')
  def get_panda_events(self):
    events = []
    if not self.cs.sm.valid['pandaStates']:
      events.append(EventName.usbError)

    for i, pandaState in enumerate(self.cs.sm['pandaStates']):
      # All pandas must match the list of safetyConfigs, and if outside this list, must be silent or noOutput
      if i < len(self.cs.CP.safetyConfigs):
        safety_mismatch = pandaState.safetyModel != self.cs.CP.safetyConfigs[i].safetyModel or \
                          pandaState.safetyParam != self.cs.CP.safetyConfigs[i].safetyParam or \
                          pandaState.alternativeExperience != self.cs.CP.alternativeExperience
      else:
        safety_mismatch = pandaState.safetyModel not in IGNORED_SAFETY_MODES

      # safety mismatch allows some time for pandad to set the safety mode and publish it back from panda
      if (safety_mismatch and self.cs.sm.frame*DT_CTRL > 10.) or pandaState.safetyRxChecksInvalid or self.cs.mismatch_counter >= 200:
        events.append(EventName.controlsMismatch)

      if log.PandaState.FaultType.relayMalfunction in pandaState.faults:
        events.append(EventName.relayMalfunction)

    return events

  @cache_function('managerState')
  def get_process_events(self):
    events = []
    not_running = {p.name for p in self.cs.sm['managerState'].processes if not p.running and p.shouldBeRunning}
    if self.cs.sm.recv_frame['managerState'] and (not_running - IGNORE_PROCESSES):
      events.append(EventName.processNotRunning)
      if not_running != self.not_running_prev:
        cloudlog.event("process_not_running", not_running=not_running, error=True)
      self.not_running_prev = not_running
    else:
      if not SIMULATION and not self.cs.rk.lagging:
        if not self.cs.sm.all_alive(self.cs.camera_packets):
          events.append(EventName.cameraMalfunction)
        elif not self.cs.sm.all_freq_ok(self.cs.camera_packets):
          events.append(EventName.cameraFrameRate)
    return events

  @cache_function('liveLocationKalman')
  def get_gps_events(self):
    events = []
    if not (self.cs.CP.notCar and self.cs.joystick_mode):
      if not self.cs.sm['liveLocationKalman'].posenetOK:
        events.append(EventName.posenetInvalid)
      if not self.cs.sm['liveLocationKalman'].deviceStable:
        events.append(EventName.deviceFalling)
      if not self.cs.sm['liveLocationKalman'].inputsOK:
        events.append(EventName.locationdTemporaryError)
      if not self.cs.sm['liveParameters'].valid and not TESTING_CLOSET and (not SIMULATION or REPLAY):
        events.append(EventName.paramsdTemporaryError)
    return events

  def update_events(self, CS):

    self.cs.events.clear()

    events = []
    # Add joystick event, static on cars, dynamic on nonCars
    if self.cs.joystick_mode:
      events.append(EventName.joystickDebug)
      self.cs.startup_event = None

    # Add startup event
    if self.cs.startup_event is not None:
      events.append(self.cs.startup_event)
      self.cs.startup_event = None

    # Don't add any more events if not initialized
    if not self.cs.initialized:
      events.append(EventName.controlsInitializing)
      return

    # no more events while in dashcam mode
    if self.cs.CP.passive:
      return

    # Block resume if cruise never previously enabled
    resume_pressed = any(be.type in (ButtonType.accelCruise, ButtonType.resumeCruise) for be in CS.buttonEvents)
    if not self.cs.CP.pcmCruise and not self.cs.v_cruise_helper.v_cruise_initialized and resume_pressed:
      events.append(EventName.resumeBlocked)

    if not self.cs.CP.notCar:
      self.cs.events.add_from_msg(self.cs.sm['driverMonitoringState'].events)

    # Add car events, ignore if CAN isn't valid
    if CS.canValid:
      self.cs.events.add_from_msg(CS.events)

    events += self.get_device_events()
    events += self.get_peripheral_events()
    events += self.get_calibration_events()
    events += self.get_lanechange_events()
    events += self.get_panda_events()

    # Handle HW and system malfunctions
    # Order is very intentional here. Be careful when modifying this.
    # All events here should at least have NO_ENTRY and SOFT_DISABLE.
    num_events = len(events)
    events += self.get_process_events()

    if not REPLAY and self.cs.rk.lagging:
      events.append(EventName.controlsdLagging)
    if len(self.cs.sm['radarState'].radarErrors) or ((not self.cs.rk.lagging or REPLAY) and not self.cs.sm.all_checks(['radarState'])):
      events.append(EventName.radarFault)
    if CS.canTimeout:
      events.append(EventName.canBusMissing)
    elif not CS.canValid:
      events.append(EventName.canError)

    # generic catch-all. ideally, a more specific event should be added above instead
    has_disable_events = self.cs.events.contains(ET.NO_ENTRY) and (self.cs.events.contains(ET.SOFT_DISABLE) or self.cs.events.contains(ET.IMMEDIATE_DISABLE))
    no_system_errors = (not has_disable_events) or (len(self.cs.events) == num_events)
    if not self.cs.sm.all_checks() and no_system_errors:
      if not self.cs.sm.all_alive():
        events.append(EventName.commIssue)
      elif not self.cs.sm.all_freq_ok():
        events.append(EventName.commIssueAvgFreq)
      else:
        events.append(EventName.commIssue)

      logs = {
        'invalid': [s for s, valid in self.cs.sm.valid.items() if not valid],
        'not_alive': [s for s, alive in self.cs.sm.alive.items() if not alive],
        'not_freq_ok': [s for s, freq_ok in self.cs.sm.freq_ok.items() if not freq_ok],
      }
      if logs != self.logged_comm_issue:
        cloudlog.event("commIssue", error=True, **logs)
        self.logged_comm_issue = logs
    else:
      self.logged_comm_issue = None

    self.get_gps_events()

    # conservative HW alert. if the data or frequency are off, locationd will throw an error
    if any((self.cs.sm.frame - self.cs.sm.recv_frame[s])*DT_CTRL > 10. for s in self.cs.sensor_packets):
      events.append(EventName.sensorDataInvalid)

    if not REPLAY:
      # Check for mismatch between openpilot and car's PCM
      cruise_mismatch = CS.cruiseState.enabled and (not self.cs.enabled or not self.cs.CP.pcmCruise)
      self.cruise_mismatch_counter = self.cruise_mismatch_counter + 1 if cruise_mismatch else 0
      if self.cruise_mismatch_counter > int(6. / DT_CTRL):
        events.append(EventName.cruiseMismatch)

    # Check for FCW
    stock_long_is_braking = self.cs.enabled and not self.cs.CP.openpilotLongitudinalControl and CS.aEgo < -1.25
    model_fcw = self.cs.sm['modelV2'].meta.hardBrakePredicted and not CS.brakePressed and not stock_long_is_braking
    planner_fcw = self.cs.sm['longitudinalPlan'].fcw and self.cs.enabled
    if (planner_fcw or model_fcw) and not (self.cs.CP.notCar and self.cs.joystick_mode):
      events.append(EventName.fcw)

    for m in messaging.drain_sock(self.cs.log_sock, wait_for_one=False):
      try:
        msg = m.androidLog.message
        if any(err in msg for err in ("ERROR_CRC", "ERROR_ECC", "ERROR_STREAM_UNDERFLOW", "APPLY FAILED")):
          csid = msg.split("CSID:")[-1].split(" ")[0]
          evt = CSID_MAP.get(csid, None)
          if evt is not None:
            events.append(evt)
      except UnicodeDecodeError:
        pass

    # TODO: fix simulator
    if not SIMULATION or REPLAY:
      # Not show in first 1 km to allow for driving out of garage. This event shows after 5 minutes
      if not self.cs.sm['liveLocationKalman'].gpsOK and self.cs.sm['liveLocationKalman'].inputsOK and (self.distance_traveled > 1500):
        events.append(EventName.noGps)
      if self.cs.sm['liveLocationKalman'].gpsOK:
        self.distance_traveled = 0
      self.distance_traveled += CS.vEgo * DT_CTRL
