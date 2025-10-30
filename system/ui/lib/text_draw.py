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


def draw_text_at(position: rl.Vector2, text: str, font_weight: FontWeight, font_size: int):
  """Draw text at a specific position."""
  font = gui_app.font(font_weight)
  rl.draw_text_ex(font, text, position, font_size, 0, rl.WHITE)

def draw_text(rect: rl.Rectangle, text: str, font_weight: FontWeight, font_size: int,
              color: rl.Color = rl.WHITE, flags: int = Alignment.LEFT | Alignment.TOP):
  """Render text with specified alignment within the given rectangle."""
  font = gui_app.font(font_weight)
  lines = wrap_text(font, text, font_size, int(rect.width)) if flags & Alignment.WORD_WRAP else [text]
  line_height = font_size * FONT_SCALE
  total_height = len(lines) * line_height

  y = rect.y
  if flags & Alignment.V_CENTER:
    y += (rect.height - total_height) / 2
  elif flags & Alignment.BOTTOM:
    y += rect.height - total_height

  for line in lines:
    text_size = measure_text_cached(font, line, font_size)
    x = rect.x
    if flags & Alignment.H_CENTER:
      x += (rect.width - text_size.x) / 2
    elif flags & Alignment.RIGHT:
      x += rect.width - text_size.x
    rl.draw_text_ex(font, line, rl.Vector2(x, y), font_size, 0, color)
    y += line_height
