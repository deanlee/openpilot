from collections.abc import Callable
from dataclasses import dataclass
import pyray as rl

from openpilot.system.ui.lib.application import gui_app, FontWeight, DEFAULT_TEXT_SIZE, DEFAULT_TEXT_COLOR, FONT_SCALE
from openpilot.system.ui.widgets import Widget
from openpilot.system.ui.lib.text_measure import measure_text_cached
from openpilot.system.ui.lib.utils import GuiStyleContext
from openpilot.system.ui.lib.wrap_text import wrap_text


# TODO: make this common
def _resolve_value(value, default=""):
  if callable(value):
    return value()
  return value if value is not None else default


def _elide_text(font: rl.Font, text: str, font_size: int, max_width: int, spacing: float) -> str:
  """Binary search eliding."""
  ellipsis = "..."
  if measure_text_cached(font, text, font_size, spacing).x <= max_width:
    return text

  left, right = 0, len(text)
  while left < right:
    mid = (left + right) // 2
    cand = text[:mid] + ellipsis
    if measure_text_cached(font, cand, font_size, spacing).x <= max_width:
      left = mid + 1
    else:
      right = mid
  return text[: left - 1] + ellipsis if left > 0 else ellipsis


def gui_label(
  rect: rl.Rectangle,
  text: str,
  font_size: int = DEFAULT_TEXT_SIZE,
  color: rl.Color = DEFAULT_TEXT_COLOR,
  font_weight: FontWeight = FontWeight.NORMAL,
  alignment: int = rl.GuiTextAlignment.TEXT_ALIGN_LEFT,
  alignment_vertical: int = rl.GuiTextAlignmentVertical.TEXT_ALIGN_MIDDLE,
  elide_right: bool = True,
):
  font = gui_app.font(font_weight)
  text_size = measure_text_cached(font, text, font_size)
  display_text = text

  # Elide text to fit within the rectangle
  if elide_right and text_size.x > rect.width:
    display_text = _elide_text(font, text, font_size, int(rect.width), 0.0)
    text_size = measure_text_cached(font, display_text, font_size)

  # Calculate horizontal position based on alignment
  text_x = rect.x + {
    rl.GuiTextAlignment.TEXT_ALIGN_LEFT: 0,
    rl.GuiTextAlignment.TEXT_ALIGN_CENTER: (rect.width - text_size.x) / 2,
    rl.GuiTextAlignment.TEXT_ALIGN_RIGHT: rect.width - text_size.x,
  }.get(alignment, 0)

  # Calculate vertical position based on alignment
  text_y = rect.y + {
    rl.GuiTextAlignmentVertical.TEXT_ALIGN_TOP: 0,
    rl.GuiTextAlignmentVertical.TEXT_ALIGN_MIDDLE: (rect.height - text_size.y) / 2,
    rl.GuiTextAlignmentVertical.TEXT_ALIGN_BOTTOM: rect.height - text_size.y,
  }.get(alignment_vertical, 0)

  # Draw the text in the specified rectangle
  # TODO: add wrapping and proper centering for multiline text
  rl.draw_text_ex(font, display_text, rl.Vector2(text_x, text_y), font_size, 0, color)


def gui_text_box(
  rect: rl.Rectangle,
  text: str,
  font_size: int = DEFAULT_TEXT_SIZE,
  color: rl.Color = DEFAULT_TEXT_COLOR,
  alignment: int = rl.GuiTextAlignment.TEXT_ALIGN_LEFT,
  alignment_vertical: int = rl.GuiTextAlignmentVertical.TEXT_ALIGN_TOP,
  font_weight: FontWeight = FontWeight.NORMAL,
  line_scale: float = 1.0,
):
  styles = [
    (rl.GuiControl.DEFAULT, rl.GuiControlProperty.TEXT_COLOR_NORMAL, rl.color_to_int(color)),
    (rl.GuiControl.DEFAULT, rl.GuiDefaultProperty.TEXT_SIZE, round(font_size * FONT_SCALE)),
    (rl.GuiControl.DEFAULT, rl.GuiDefaultProperty.TEXT_LINE_SPACING, round(font_size * FONT_SCALE * line_scale)),
    (rl.GuiControl.DEFAULT, rl.GuiControlProperty.TEXT_ALIGNMENT, alignment),
    (rl.GuiControl.DEFAULT, rl.GuiDefaultProperty.TEXT_ALIGNMENT_VERTICAL, alignment_vertical),
    (rl.GuiControl.DEFAULT, rl.GuiDefaultProperty.TEXT_WRAP_MODE, rl.GuiTextWrapMode.TEXT_WRAP_WORD),
  ]
  if font_weight != FontWeight.NORMAL:
    rl.gui_set_font(gui_app.font(font_weight))

  with GuiStyleContext(styles):
    rl.gui_label(rect, text)

  if font_weight != FontWeight.NORMAL:
    rl.gui_set_font(gui_app.font(FontWeight.NORMAL))


@dataclass
class TextLine:
  text: str
  size: rl.Vector2


@dataclass
class LayoutCache:
  text: str
  width: int
  lines: list[TextLine]
  total_height: float


class Label(Widget):
  def __init__(
    self,
    text: str | Callable[[], str],
    font_size: int = DEFAULT_TEXT_SIZE,
    font_weight: FontWeight = FontWeight.NORMAL,
    text_color: rl.Color = DEFAULT_TEXT_COLOR,
    alignment: int = rl.GuiTextAlignment.TEXT_ALIGN_LEFT,
    alignment_vertical: int = rl.GuiTextAlignmentVertical.TEXT_ALIGN_TOP,
    text_padding: int = 0,
    max_width: int | None = None,
    elide: bool = True,
    wrap_text: bool = True,
    line_height: float = 1.0,
    letter_spacing: float = 0.0,
  ):
    super().__init__()
    self._text = text
    self._font_size = font_size
    self._font_weight = font_weight
    self._text_color = text_color
    self._align = alignment
    self._align_v = alignment_vertical
    self._text_padding = text_padding
    self._max_width = max_width
    self._elide = elide
    self._wrap_text = wrap_text
    self._line_height = line_height * 0.9
    self._spacing = font_size * letter_spacing

    self._cache: LayoutCache | None = None

    # If max_width is set, initialize rect size for Scroller support
    if max_width is not None:
      self._rect.width = max_width
      self._rect.height = self.get_content_height(max_width)

  def set_text(self, text: str | Callable[[], str]):
    self._text = text
    if self._cache and self._cache.text != str(_resolve_value(text)):
      self._cache = None

  @property
  def text(self) -> str:
    return str(_resolve_value(self._text))

  def set_text_color(self, color: rl.Color):
    self._text_color = color

  def set_color(self, color: rl.Color):
    self.set_text_color(color)

  @property
  def font_size(self) -> int:
    return self._font_size

  def set_font_size(self, size: int):
    if self._font_size != size:
      self._font_size = size
      self._cache = None

  def set_font_weight(self, font_weight: FontWeight):
    if self._font_weight != font_weight:
      self._font_weight = font_weight
      self._cache = None

  def set_alignment(self, alignment: int):
    self._align = alignment

  def set_alignment_vertical(self, alignment_vertical: int):
    self._align_v = alignment_vertical

  def set_max_width(self, max_width: int | None):
    if self._max_width != max_width:
      self._max_width = max_width
      self._cache = None
      if max_width is not None:
        self._rect.width = max_width
        self._rect.height = self.get_content_height(max_width)

  def _update_text_cache(self, available_width: int):
    text = self.text
    # Check if cache is still valid
    if self._cache and self._cache.text == text and self._cache.width == available_width:
      return

    # Determine wrapping width
    content_width = available_width - (self._text_padding * 2)
    if content_width <= 0:
      content_width = 1

    font = gui_app.font(self._font_weight)
    if self._wrap_text:
      raw_lines = wrap_text(font, text, self._font_size, content_width, self._spacing)
    else:
      raw_lines = text.split('\n') if text else [""]

    # Process each line
    lines: list[TextLine] = []
    total_height = 0.0

    for idx, line in enumerate(raw_lines):
      if not line:
        size = rl.Vector2(0, self._font_size * self._line_height)
      else:
        if self._elide and not self._wrap_text:  # Elide usually only applies to single lines or specific fixed widths
          # Note: Full multi-line elision (last line only) requires context of height,
          # simplified here to line-by-line check or usually just single line mode.
          line = _elide_text(font, line, self._font_size, available_width, self._spacing)
      size = measure_text_cached(font, line, self._font_size, self._spacing)
      lines.append(TextLine(line, size))
      total_height += size.y if idx == 0 else (self._font_size * self._line_height)

    self._cache = LayoutCache(text=text, width=available_width, lines=lines, total_height=total_height)

  def get_content_height(self, max_width: int = 0) -> float:
    width = max_width if max_width else self._max_width
    self._update_text_cache(int(width))

    return 0 if not self._cache else self._cache.total_height

  def _render(self, _):
    if self._rect.width <= 0 or self._rect.height <= 0:
      return

    # Determine available width
    available_width = self._rect.width
    if self._max_width is not None:
      available_width = min(available_width, self._max_width)

    # Update text cache
    self._update_text_cache(int(available_width))

    # Vertical clipping & alignment
    visible_block_height = min(self._rect.height, self._cache.total_height)
    if self._align_v == rl.GuiTextAlignmentVertical.TEXT_ALIGN_TOP:
      current_y = self._rect.y
    elif self._align_v == rl.GuiTextAlignmentVertical.TEXT_ALIGN_BOTTOM:
      current_y = self._rect.y + self._rect.height - visible_block_height
    else:  # TEXT_ALIGN_MIDDLE
      current_y = self._rect.y + (self._rect.height - visible_block_height) / 2

    font = gui_app.font(self._font_weight)

    for line in self._cache.lines:
      if self._align == rl.GuiTextAlignment.TEXT_ALIGN_LEFT:
        line_x = self._rect.x + self._text_padding
      elif self._align == rl.GuiTextAlignment.TEXT_ALIGN_CENTER:
        line_x = self._rect.x + (self._rect.width - line.size.x) / 2
      else:  # TEXT_ALIGN_RIGHT
        line_x = self._rect.x + self._rect.width - line.size.x - self._text_padding

      line_pos = rl.Vector2(line_x, current_y)
      rl.draw_text_ex(font, line.text, line_pos, self._font_size, self._spacing, self._text_color)
      current_y += self._font_size * self._line_height
