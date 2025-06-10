from dataclasses import dataclass
import pyray as rl
from pyray import Rectangle


@dataclass
class MouseState:
  def __init__(self):
    self.reset()
    self._position = rl.Vector2(0, 0, 0, 0)
    self._released: bool = False
    self._consumed: bool = False
    self._pressed_pos = rl.Vector2(0, 0, 0, 0)
    self._is_down: bool = False

  def reset(self):
    self._consumed = False
    self._position = rl.get_mouse_position()
    self._pressed = rl.is_mouse_button_pressed(rl.MouseButton.MOUSE_BUTTON_LEFT)
    self._released = rl.is_mouse_button_released(rl.MouseButton.MOUSE_BUTTON_LEFT)
    self._is_down = rl.is_mouse_button_down(rl.MouseButton.MOUSE_BUTTON_LEFT)

    if self._pressed:
      self._pressed_pos = self._position

  @property
  def position(self) -> rl.Vector2:
    return self._position

  @property
  def pressed(self) -> bool:
    return self._pressed and not self._consumed

  def pressed_in_rect(self, rect: rl.Rectangle) -> bool:
    if self._pressed and not self._consumed:
      if rl.check_collision_point_rec(self._position, rect):
        # self._consumed = True
        return True
    return False

  def released_in_rect(self, rect: rl.Rectangle) -> bool:
    print("consumed:", self._consumed)
    if self._released and not self._consumed:
      if rl.check_collision_point_rec(self._position, rect):
        # self._consumed = True
        return True
    return False

  @property
  def released(self) -> bool:
    return self._released and not self._consumed

  def is_clicked(self, rect: rl.Rectangle) -> bool:
    if self.released and self._pressed_pos is not None:
      if rl.check_collision_point_rec(self._position, rect) and rl.check_collision_point_rec(self._pressed_pos, rect):
        self._consumed = True
        return True
    return False

  def is_in_rect(self, rect: rl.Rectangle) -> bool:
    return rl.check_collision_point_rec(self._position, rect) and not self._consumed

  def is_down(self) -> bool:
    return self._is_down and not self._consumed


  def is_down_in_rect(self, rect: rl.Rectangle) -> bool:
    return self._is_down and not self._consumed and rl.check_collision_point_rec(self._position, rect)

  @property
  def consumed(self) -> bool:
    return self._consumed

  @consumed.setter
  def consumed(self, value: bool):
    self._consumed = True
