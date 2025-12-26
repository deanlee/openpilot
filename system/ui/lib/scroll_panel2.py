import os
import math
import pyray as rl
from collections.abc import Callable
from enum import Enum
from typing import cast
from openpilot.system.ui.lib.application import gui_app, MouseEvent
from openpilot.system.hardware import TICI
from collections import deque

MIN_VELOCITY = 10  # px/s, changes from auto scroll to steady state
MIN_VELOCITY_FOR_CLICKING = 2 * 60  # px/s, accepts clicks while auto scrolling below this velocity
MIN_DRAG_PIXELS = 12
AUTO_SCROLL_TC_SNAP = 0.025
AUTO_SCROLL_TC = 0.18
BOUNCE_RETURN_RATE = 10.0
REJECT_DECELERATION_FACTOR = 3
MAX_SPEED = 10000.0  # px/s
CLICK_VELOCITY_LIMIT = 120.0  # px/s

DEBUG = os.getenv("DEBUG_SCROLL", "0") == "1"


# from https://ariya.io/2011/10/flick-list-with-its-momentum-scrolling-and-deceleration
class ScrollState(Enum):
  STEADY = 0
  PRESSED = 1
  MANUAL_SCROLL = 2
  AUTO_SCROLL = 3


class GuiScrollPanel2:
  def __init__(self, horizontal: bool = True, handle_out_of_bounds: bool = True) -> None:
    self._horizontal = horizontal
    self._handle_out_of_bounds = handle_out_of_bounds
    self._AUTO_SCROLL_TC = AUTO_SCROLL_TC_SNAP if not self._handle_out_of_bounds else AUTO_SCROLL_TC
    self._state = ScrollState.STEADY
    self._offset: float = 0.0
    self._initial_click_event: MouseEvent | None = None
    self._prev_mouse: MouseEvent | None = None
    self._velocity = 0.0  # pixels per second
    self._vel_buffer: deque[float] = deque(maxlen=12 if TICI else 6)
    self._enabled: bool | Callable[[], bool] = True

  def set_enabled(self, enabled: bool | Callable[[], bool]) -> None:
    self._enabled = enabled

  @property
  def enabled(self) -> bool:
    return self._enabled() if callable(self._enabled) else self._enabled

  def update(self, bounds: rl.Rectangle, content_size: float) -> float:
    if DEBUG:
      print('Old state:', self._state)

    if not self.enabled:
      # Reset state if not enabled
      self._state = ScrollState.STEADY
      self._velocity = 0.0
      self._vel_buffer.clear()
      return self._offset

    bounds_size = bounds.width if self._horizontal else bounds.height
    max_offset, min_offset = 0.0, min(0.0, bounds_size - content_size)
    for mouse_event in gui_app.mouse_events:
      self._handle_mouse_event(mouse_event, bounds, max_offset, min_offset)
      self._prev_mouse = mouse_event

    if self._state == ScrollState.AUTO_SCROLL:
      self._step_physics(max_offset, min_offset)

    if DEBUG:
      print(f"State: {self._state} | Off: {self._offset:.1f} | Vel: {self._velocity:.1f}")
    return self._offset

  def _handle_mouse_event(self, mouse_event: MouseEvent, bounds: rl.Rectangle, max_offset:float, min_offset: float) -> None:
    out_of_bounds = self._offset > max_offset or self._offset < min_offset
    mouse_pos = self._get_mouse_pos(mouse_event)

    if self._state == ScrollState.STEADY:
      if mouse_event.left_pressed and rl.check_collision_point_rec(mouse_event.pos, bounds):
        self._state = ScrollState.PRESSED
        self._initial_click_event = mouse_event

    elif self._state == ScrollState.PRESSED:
      initial_click_pos = self._get_mouse_pos(cast(MouseEvent, self._initial_click_event))
      diff = abs(mouse_pos - initial_click_pos)
      if mouse_event.left_released:
        # Special handling for down and up clicks across two frames
        # TODO: not sure what that means or if it's accurate anymore
        if out_of_bounds:
          self._state = ScrollState.AUTO_SCROLL
        elif diff <= MIN_DRAG_PIXELS:
          self._state = ScrollState.STEADY
        else:
          self._state = ScrollState.MANUAL_SCROLL
      elif diff > MIN_DRAG_PIXELS:
        self._state = ScrollState.MANUAL_SCROLL

    elif self._state == ScrollState.MANUAL_SCROLL:
      if mouse_event.left_released:
        self._finalize_manual_scroll(out_of_bounds)
      else:
        self._update_manual_drag(mouse_pos, mouse_event.t, out_of_bounds)

    elif self._state == ScrollState.AUTO_SCROLL:
      if mouse_event.left_pressed:
        # Decide whether to click or scroll (block click if moving too fast)
        if abs(self._velocity) <= MIN_VELOCITY_FOR_CLICKING:
          # Traveling slow enough, click
          self._state = ScrollState.PRESSED
          self._initial_click_event = mouse_event
        else:
          # Go straight into manual scrolling to block erroneous input
          self._state = ScrollState.MANUAL_SCROLL
          # Reset velocity for touch down and up events that happen in back-to-back frames
          self._velocity = 0.0

  def _update_manual_drag(self, m_pos: float, m_t: float, is_oob: bool):
    if not self._prev_mouse:
      return

    delta = m_pos - self._get_mouse_pos(self._prev_mouse)
    dt = max(m_t - self._prev_mouse.t, 1e-6)

    self._velocity = max(-MAX_SPEED, min(MAX_SPEED, delta / dt))
    self._vel_buffer.append(self._velocity)

    # Rubber-banding
    if is_oob and self._handle_out_of_bounds:
      delta *= 0.25
    self._offset += delta

  def _finalize_manual_scroll(self, is_oob: bool):
    # Determine if flick is valid or jitter rejection
    low_speed = abs(self._velocity) < CLICK_VELOCITY_LIMIT * 1.5
    # Rejection logic: if velocity dropped sharply before release, it's a stop, not a flick
    high_decel = False
    if len(self._vel_buffer) >= 4:
      avg_start = abs(sum(list(self._vel_buffer)[:len(self._vel_buffer)//2]))
      if abs(self._velocity) * 3.0 < avg_start:
        high_decel = True

    if is_oob or not (high_decel or low_speed):
      self._state = ScrollState.AUTO_SCROLL
    else:
      self._state = ScrollState.STEADY
      self._velocity = 0.0
    self._vel_buffer.clear()

  def _step_physics(self, max_offset: float, min_offset: float) -> None:
    """Runs per render frame, independent of mouse events. Updates auto-scrolling state and velocity."""
    # simple exponential return if out of bounds
    dt = rl.get_frame_time() or 1e-6
    out_of_bounds = self._offset > max_offset or self._offset < min_offset
    if out_of_bounds and self._handle_out_of_bounds:
      target = max_offset if self._offset > max_offset else min_offset
      factor = 1.0 - math.exp(-BOUNCE_RETURN_RATE * dt)

      dist = target - self._offset
      self._offset += dist * factor  # ease toward the edge
      self._velocity *= (1.0 - factor)  # damp any leftover fling

      # Steady once we are close enough to the target
      if abs(dist) < 1 and abs(self._velocity) < MIN_VELOCITY:
        self._offset = target
        self._velocity = 0.0
        self._state = ScrollState.STEADY

    elif abs(self._velocity) < MIN_VELOCITY:
      self._velocity = 0.0
      self._state = ScrollState.STEADY

    # Update the offset based on the current velocity
    self._offset += self._velocity * dt  # Adjust the offset based on velocity
    alpha = 1 - (dt / (self._AUTO_SCROLL_TC + dt))
    self._velocity *= alpha

  def _get_mouse_pos(self, mouse_event: MouseEvent) -> float:
    return mouse_event.pos.x if self._horizontal else mouse_event.pos.y

  def get_offset(self) -> float:
    return self._offset

  def set_offset(self, value: float) -> None:
      self._offset = value

  @property
  def state(self) -> ScrollState:
    return self._state

  def is_touch_valid(self) -> bool:
    # MIN_VELOCITY_FOR_CLICKING is checked in auto-scroll state
    return bool(self._state != ScrollState.MANUAL_SCROLL)
