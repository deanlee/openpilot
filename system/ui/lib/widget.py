import abc
import pyray as rl
from enum import IntEnum
from openpilot.system.ui.lib.application import mouse
from openpilot.system.ui.lib.mouse_state import MouseState


class DialogResult(IntEnum):
  CANCEL = 0
  CONFIRM = 1
  NO_ACTION = -1


class Widget(abc.ABC):
  def __init__(self):
    pass

  def render(self, rect: rl.Rectangle) -> bool | int | None:
    ret = self._render(rect)

    if mouse.pressed_in_rect(rect):
      if self._on_mouse_pressed(mouse):
        mouse._consumed = True

    if mouse.check_clicked(rect):
      if self._on_click(mouse):
        mouse._consumed = True
    return ret

  @abc.abstractmethod
  def _render(self, rect: rl.Rectangle) -> bool | int | None:
    """Render the widget within the given rectangle."""

  def _on_click(self, mouse: MouseState) -> int:
    """Handle mouse release events, if applicable."""
    return -1

  def _on_mouse_pressed(self, mouse: MouseState) -> int:
    """Handle mouse pressed events, if applicable."""
    return -1
