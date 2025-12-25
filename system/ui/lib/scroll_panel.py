import os
import math
import pyray as rl
from collections import deque
from collections.abc import Callable
from enum import Enum
from typing import Optional
from openpilot.system.ui.lib.application import gui_app, MouseEvent
from openpilot.system.hardware import TICI

FRICTION_TIME_CONSTANT = 0.18
BOUNCE_RETURN_STRENGTH = 12.0
RUBBER_BAND_RESISTANCE = 0.4
MAX_OVERSHOOT = 200.0

MOUSE_WHEEL_SPEED = 80.0

MIN_DRAG_THRESHOLD = 10
MIN_FLING_VELOCITY = 60.0
MIN_VELOCITY_FOR_CLICK_BLOCK = 180.0
MAX_VELOCITY = 15000.0

VELOCITY_HISTORY_SIZE = 12 if TICI else 8
DEBUG = os.getenv("DEBUG_SCROLL", "0") == "1"


class ScrollState(Enum):
  IDLE = 0
  PRESSED = 1
  DRAGGING = 2
  INERTIA = 3


class GuiScrollPanel:
  def __init__(self, horizontal: bool = True, handle_out_of_bounds: bool = True):
    self._horizontal = horizontal
    self._handle_out_of_bounds = handle_out_of_bounds

    self._state = ScrollState.IDLE
    self._offset = rl.Vector2(0.0, 0.0)

    self._velocity = 0.0
    self._velocity_history = deque[float](maxlen=VELOCITY_HISTORY_SIZE)

    self._drag_start_offset = 0.0
    self._drag_start_pos = 0.0
    self._last_pos = 0.0
    self._last_time = 0.0

    self._enabled: bool | Callable[[], bool] = True
    self._wheel_accum = 0.0

  def set_enabled(self, enabled: bool | Callable[[], bool]) -> None:
    self._enabled = enabled
    if not self.enabled:
      self._reset_to_idle()

  @property
  def enabled(self) -> bool:
    return self._enabled() if callable(self._enabled) else self._enabled

  def update(self, bounds: rl.Rectangle, content_size: float) -> float:
    if not self.enabled:
      self._reset_to_idle()

    bounds_size = bounds.width if self._horizontal else bounds.height
    max_offset = 0.0
    min_offset = min(0.0, bounds_size - content_size)

    self._process_inputs(bounds, max_offset, min_offset)
    self._update_physics(max_offset, min_offset)

    if DEBUG:
      print(f"[Scroll] State={self._state.name} Offset={self.get_offset():.1f} Vel={self._velocity:.1f}")

    return self.get_offset()

  def _process_inputs(self, bounds: rl.Rectangle, max_offset: float, min_offset: float) -> None:
    latest_down_pos: Optional[float] = None
    latest_down_time: float = 0.0

    for event in gui_app.mouse_events:
      if event.slot != 0:
        continue

      pos = self._get_mouse_pos(event)

      if event.left_pressed:
        if rl.check_collision_point_rec(event.pos, bounds):
          self._state = ScrollState.PRESSED
          self._drag_start_offset = self.get_offset()
          self._drag_start_pos = pos
          self._last_pos = pos
          self._last_time = event.t
          self._velocity = 0.0
          self._velocity_history.clear()

      elif event.left_released:
        if self._state in (ScrollState.PRESSED, ScrollState.DRAGGING):
          self._handle_release()

      elif event.left_down and self._state in (ScrollState.PRESSED, ScrollState.DRAGGING):
        latest_down_pos = pos
        latest_down_time = event.t

    if latest_down_pos is not None:
      self._apply_latest_position(latest_down_pos, latest_down_time, max_offset, min_offset)

    # Wheel only when not touching
    if self._state not in (ScrollState.PRESSED, ScrollState.DRAGGING):
      wheel = rl.get_mouse_wheel_move()
      if wheel != 0:
        delta = wheel * MOUSE_WHEEL_SPEED
        self._wheel_accum += delta
        apply = math.copysign(math.floor(abs(self._wheel_accum)), self._wheel_accum)
        if apply != 0:
          self._offset_add(apply)
          self._wheel_accum -= apply
          self._velocity += apply * gui_app.target_fps * 0.5

  def _apply_latest_position(self, pos: float, time: float, max_offset: float, min_offset: float) -> None:
    dt = max(time - self._last_time, 1e-6)
    raw_delta = pos - self._last_pos

    moved_distance = abs(pos - self._drag_start_pos)

    if self._state == ScrollState.PRESSED:
      if moved_distance < MIN_DRAG_THRESHOLD:
        # Still deciding — do NOT move content
        self._last_pos = pos
        self._last_time = time
        return

      # Threshold crossed → begin dragging
      self._state = ScrollState.DRAGGING
      # Content stays put until now — no sudden jump

    # === DRAGGING: follow finger with rubber banding ===
    desired_offset = self._drag_start_offset + (pos - self._drag_start_pos)

    overshoot = 0.0
    if desired_offset > max_offset:
      overshoot = desired_offset - max_offset
    elif desired_offset < min_offset:
      overshoot = desired_offset - min_offset

    resistance = 1.0
    if overshoot != 0:
      factor = min(1.0, abs(overshoot) / MAX_OVERSHOOT)
      resistance = 1.0 - (RUBBER_BAND_RESISTANCE * factor)

    final_delta = raw_delta * resistance
    self._offset_add(final_delta)

    velocity = final_delta / dt
    velocity = max(-MAX_VELOCITY, min(MAX_VELOCITY, velocity))
    self._velocity = velocity
    self._velocity_history.append(velocity)

    self._last_pos = pos
    self._last_time = time

  def _handle_release(self) -> None:
    if len(self._velocity_history) >= 3:
      recent_vel = sum(list(self._velocity_history)[-3:]) / 3.0
    elif self._velocity_history:
      recent_vel = self._velocity_history[-1]
    else:
      recent_vel = 0.0

    if abs(recent_vel) >= MIN_FLING_VELOCITY:
      self._velocity = recent_vel
      self._state = ScrollState.INERTIA
    else:
      self._velocity = 0.0
      self._state = ScrollState.IDLE

    self._velocity_history.clear()

  def _update_physics(self, max_offset: float, min_offset: float) -> None:
    dt = rl.get_frame_time()
    if dt <= 0:
      dt = 1.0 / gui_app.target_fps

    current = self.get_offset()

    if self._state == ScrollState.INERTIA:
      decay = math.exp(-dt / FRICTION_TIME_CONSTANT)
      self._velocity *= decay
      self._offset_add(self._velocity * dt)

      if abs(self._velocity) < 15.0:
        self._velocity = 0.0
        self._state = ScrollState.IDLE

    if self._handle_out_of_bounds:
      if current > max_offset:
        overshoot = current - max_offset
        self._offset_add(-overshoot * BOUNCE_RETURN_STRENGTH * dt)
        self._velocity *= 0.85
        if overshoot < 2.0 and abs(self._velocity) < 30:
          self.set_offset(max_offset)
          self._reset_to_idle()

      elif current < min_offset:
        overshoot = current - min_offset
        self._offset_add(-overshoot * BOUNCE_RETURN_STRENGTH * dt)
        self._velocity *= 0.85
        if abs(overshoot) < 2.0 and abs(self._velocity) < 30:
          self.set_offset(min_offset)
          self._reset_to_idle()

  def _offset_add(self, delta: float) -> None:
    if self._horizontal:
      self._offset.x += delta
    else:
      self._offset.y += delta

  def _get_mouse_pos(self, event: MouseEvent) -> float:
    return event.pos.x if self._horizontal else event.pos.y

  def get_offset(self) -> float:
    return self._offset.x if self._horizontal else self._offset.y

  def set_offset(self, value: float) -> None:
    if self._horizontal:
      self._offset.x = value
    else:
      self._offset.y = value

  def _reset_to_idle(self) -> None:
    self._state = ScrollState.IDLE
    self._velocity = 0.0
    self._velocity_history.clear()

  @property
  def state(self) -> ScrollState:
    return self._state

  def is_touch_valid(self) -> bool:
    if self._state == ScrollState.DRAGGING:
      return False
    if self._state == ScrollState.INERTIA:
      return abs(self._velocity) < MIN_VELOCITY_FOR_CLICK_BLOCK
    return True  # IDLE or PRESSED
