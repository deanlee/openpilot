import abc
import pyray as rl
from enum import IntEnum
from openpilot.system.ui.lib.application import mouse


class DialogResult(IntEnum):
  CANCEL = 0
  CONFIRM = 1
  NO_ACTION = -1


class Widget(abc.ABC):
  def __init__(self):
    pass

  def render(self, rect: rl.Rectangle) -> bool | int | None:
    ret = self._render(rect)

    # Keep track of whether mouse down started within the widget's rectangle
    if mouse.check_clicked(rect):
        self._onclick()
    return ret

  @abc.abstractmethod
  def _render(self, rect: rl.Rectangle) -> bool | int | None:
    """Render the widget within the given rectangle."""

  def _on_click(self) -> int:
    """Handle mouse release events, if applicable."""
    return -1
