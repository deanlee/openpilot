from dataclasses import dataclass
import pyray as rl


@dataclass
class MouseState:
  def __init__(self):
    self.reset()
    self._position = rl.Vector2(0, 0, 0, 0)
    self._released: bool = False
    self._consumed: bool = False
    self._pressed_pos = rl.Vector2(0, 0, 0, 0)

  def reset(self):
    self._consumed = False
    self._position = rl.get_mouse_position()
    self._pressed = rl.is_mouse_button_pressed(rl.MouseButton.MOUSE_BUTTON_LEFT)
    self._released = rl.is_mouse_button_released(rl.MouseButton.MOUSE_BUTTON_LEFT)
    if self._pressed:
      self._pressed_pos = rl.get_mouse_position()
    else:
      self._pressed_pos = None

  @property
  def pressed(self) -> bool:
    return self._pressed and not self._consumed

  @property
  def released(self) -> bool:
    return self._released and not self._consumed

  def is_clicked(self, rect: rl.Rectangle) -> bool:
    if self.released and self._pressed_pos is not None:
      if rl.check_collision_point_rec(self._position, rect) and rl.check_collision_point_rec(self._pressed_pos, rect):
        self._consumed = True
      return True
    return False

  @property
  def position(self) -> rl.Vector2:
    return self._position

  @property
  def consumed(self) -> bool:
    return self._consumed

  @consumed.setter
  def consumed(self, value: bool):
    self._consumed = True


mouse_state = MouseState()
