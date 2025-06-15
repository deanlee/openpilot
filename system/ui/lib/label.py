import pyray as rl
from enum import IntEnum
from openpilot.system.ui.lib.application import gui_app, FontWeight, DEFAULT_TEXT_SIZE, DEFAULT_TEXT_COLOR
from openpilot.system.ui.lib.text_measure import measure_text_cached
from openpilot.system.ui.lib.wrap_text import wrap_text

DEFAULT_LINE_SPACING_RATIO = 0.2  # Default line spacing as fraction of font size


class Align(IntEnum):
  LEFT = 0
  CENTER = 1
  RIGHT = 2


class VAlign(IntEnum):
  TOP = 0
  MIDDLE = 1
  BOTTOM = 2


def get_wrapped_text_height(font_weight: FontWeight, text: str, font_size: int, width: float, line_space: int = 0) -> int:
  font = gui_app.font(font_weight)
  lines = wrap_text(font, text, font_size, int(width))
  line_space = line_space if line_space > 0 else int(font_size * DEFAULT_LINE_SPACING_RATIO)
  return len(lines) * font_size + (len(lines) - 1) * line_space


def gui_label(rect: rl.Rectangle, text: str, font_size: int = DEFAULT_TEXT_SIZE, color: rl.Color = DEFAULT_TEXT_COLOR,
              font_weight: FontWeight = FontWeight.NORMAL, align: int = Align.LEFT, valign: int = VAlign.MIDDLE,
              wrap: bool = False, line_space: int = 0, elide: bool = False) -> int:
  if not text:
    return 0

  font = gui_app.font(font_weight)
  line_space = line_space if line_space > 0 else int(font_size * DEFAULT_LINE_SPACING_RATIO)

  # Handle single-line (elided or unwrapped) or multi-line (wrapped) text
  if wrap:
    # If wrapping is enabled, use the wrap_text function
    lines = wrap_text(font, text, font_size, int(rect.width))
    height = height = len(lines) * font_size + line_space * (len(lines) - 1)
  else:
    lines = [text]
    height = measure_text_cached(font, text, font_size).y

  # Calculate vertical position
  if valign == VAlign.TOP:
    y = rect.y
  elif valign == VAlign.MIDDLE:
    y = rect.y + (rect.height - height) / 2
  else:  # TEXT_ALIGN_BOTTOM
    y = rect.y + rect.height - height

  # Draw each line
  for line in lines:
    if elide and measure_text_cached(font, line, font_size).x > rect.width:
      ellipsis = "..."
      for i in range(len(line), 0, -1):
        candidate = line[:i] + ellipsis
        if measure_text_cached(font, candidate, font_size).x <= rect.width:
          line = candidate
          break
      else:
        line = ellipsis

    # Calculate horizontal position
    text_width = measure_text_cached(font, line, font_size).x
    if align == Align.LEFT:
      x = rect.x
    elif align == Align.CENTER:
      x = rect.x + (rect.width - text_width) / 2
    else:  # TEXT_ALIGN_RIGHT
      x = rect.x + rect.width - text_width

    rl.draw_text_ex(font, line, rl.Vector2(x, y), font_size, 0, color)
    y += font_size + line_space

  return height


def gui_text_box(rect: rl.Rectangle, text: str, font_size: int = DEFAULT_TEXT_SIZE, color: rl.Color = DEFAULT_TEXT_COLOR,
                 align: int = Align.LEFT, valign: int = VAlign.TOP, font_weight: FontWeight = FontWeight.NORMAL) -> int:
  return gui_label(rect, text, font_size, color, font_weight, align=align, valign=valign, wrap=True)


# Convenience functions for common alignments
def draw_text_left(font_weight: FontWeight, text: str, rect: rl.Rectangle, font_size: int, color: rl.Color) -> int:
  return gui_label(rect, text, font_size, color, font_weight)


def draw_text_center(font_weight: FontWeight, text: str, rect: rl.Rectangle, font_size: int, color: rl.Color) -> int:
  return gui_label(rect, text, font_size, color, font_weight, align=Align.CENTER, valign=VAlign.MIDDLE)


def draw_text_right(font_weight: FontWeight, text: str, rect: rl.Rectangle, font_size: int, color: rl.Color) -> int:
  return gui_label(rect, text, font_size, color, font_weight, align=Align.RIGHT, valign=VAlign.MIDDLE)


def draw_wrapped_text(font_weight: FontWeight, text: str, x: float, y: float, width: float, font_size: int,
                      color: rl.Color = rl.WHITE, line_space: int = 0) -> int:
  return gui_label(rl.Rectangle(x, y, width, 0), text, font_size, color, font_weight,
                   align=Align.LEFT, valign=VAlign.TOP, wrap=True, line_space=line_space)
