import pyray as rl
from enum import IntFlag
from openpilot.system.ui.lib.text_measure import measure_text_cached
from openpilot.system.ui.lib.wrap_text import wrap_text
from openpilot.system.ui.lib.application import gui_app, FontWeight, FONT_SCALE

class Alignment(IntFlag):
  LEFT = 0x1
  H_CENTER = 0x2
  RIGHT = 0x4
  TOP = 0x8
  V_CENTER = 0x10
  BOTTOM = 0x20
  WORD_WRAP = 0x40
  CENTER = H_CENTER | V_CENTER


def draw_text(rect: rl.Rectangle, text: str, font_weight: FontWeight, font_size: int,
              color: rl.Color = rl.WHITE, flags: int = Alignment.LEFT | Alignment.TOP):
  """Render text with specified alignment within the given rectangle."""
  font = gui_app.font(font_weight)
  if flags & Alignment.WORD_WRAP:
    lines = wrap_text(font, text, font_size, int(rect.width))
  else:
    lines = [text]

  line_height = font_size * FONT_SCALE
  total_height = len(lines) * line_height

  # Vertical alignment of text block
  if flags & Alignment.V_CENTER:
    start_y = rect.y + (rect.height - total_height) / 2
  elif flags & Alignment.BOTTOM:
    start_y = rect.y + rect.height - total_height
  else:  # TOP
    start_y = rect.y

  # Draw each line
  y = start_y
  for line in lines:
    text_size = measure_text_cached(font, line, font_size)

    # Horizontal alignment per line
    if flags & Alignment.H_CENTER:
      x = rect.x + (rect.width - text_size.x) / 2
    elif flags & Alignment.RIGHT:
      x = rect.x + rect.width - text_size.x
    else:  # LEFT
      x = rect.x

    rl.draw_text_ex(font, line, rl.Vector2(x, y), font_size, 0, color)
    y += line_height


def draw_text_center(rect: rl.Rectangle, text: str, font: rl.Font, font_size: int, color: rl.Color):
  """Draw text centered within the given rectangle."""
  text_size = measure_text_cached(font, text, font_size)
  text_x = rect.x + (rect.width - text_size.x) / 2
  text_y = rect.y + (rect.height - text_size.y) / 2
  rl.draw_text_ex(font, text, rl.Vector2(text_x, text_y), font_size, 0, color)


def draw_text_right(rect: rl.Rectangle, text: str, font: rl.Font, font_size: int, color: rl.Color):
  """Draw text right-aligned within the given rectangle."""
  text_size = measure_text_cached(font, text, font_size)
  text_x = rect.x + rect.width - text_size.x
  text_y = rect.y + (rect.height - text_size.y) / 2
  rl.draw_text_ex(font, text, rl.Vector2(text_x, text_y), font_size, 0, color)


def draw_text_left(rect: rl.Rectangle, text: str, font: rl.Font, font_size: int, color: rl.Color):
  """Draw text left-aligned within the given rectangle."""
  text_size = measure_text_cached(font, text, font_size)
  text_x = rect.x
  text_y = rect.y + (rect.height - text_size.y) / 2
  rl.draw_text_ex(font, text, rl.Vector2(text_x, text_y), font_size, 0, color)
