import pyray as rl
from enum import Enum
from openpilot.system.ui.lib.text_measure import measure_text_cached


class TextAlignment(Enum):
  LEFT = "left"
  CENTER = "center"
  RIGHT = "right"


def draw_text(rect: rl.Rectangle, text: str, font: rl.Font, font_size: int,
              color: rl.Color, alignment: TextAlignment = TextAlignment.LEFT):
  """Render text with specified alignment within the given rectangle."""
  text_size = measure_text_cached(font, text, font_size)

  if alignment == TextAlignment.CENTER:
    text_x = rect.x + (rect.width - text_size.x) / 2
  elif alignment == TextAlignment.RIGHT:
    text_x = rect.x + rect.width - text_size.x
  else:  # LEFT
    text_x = rect.x

  text_y = rect.y + (rect.height - text_size.y) / 2
  rl.draw_text_ex(font, text, rl.Vector2(text_x, text_y), font_size, 0, color)


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
