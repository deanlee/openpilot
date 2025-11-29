from enum import IntEnum
from collections.abc import Callable
from dataclasses import dataclass
from typing import Union
import pyray as rl

from openpilot.system.ui.lib.application import gui_app, FontWeight, DEFAULT_TEXT_SIZE, DEFAULT_TEXT_COLOR, FONT_SCALE
from openpilot.system.ui.widgets import Widget
from openpilot.system.ui.lib.text_measure import measure_text_cached
from openpilot.system.ui.lib.utils import GuiStyleContext
from openpilot.system.ui.lib.emoji import find_emoji, emoji_tex
from openpilot.system.ui.lib.wrap_text import wrap_text

ICON_PADDING = 15


# TODO: make this common
def _resolve_value(value, default=""):
  if callable(value):
    return value()
  return value if value is not None else default


class ScrollState(IntEnum):
  STARTING = 0
  SCROLLING = 1


# TODO: This should be a Widget class
def gui_label(
  rect: rl.Rectangle,
  text: str,
  font_size: int = DEFAULT_TEXT_SIZE,
  color: rl.Color = DEFAULT_TEXT_COLOR,
  font_weight: FontWeight = FontWeight.NORMAL,
  alignment: int = rl.GuiTextAlignment.TEXT_ALIGN_LEFT,
  alignment_vertical: int = rl.GuiTextAlignmentVertical.TEXT_ALIGN_MIDDLE,
  elide_right: bool = True
):
  font = gui_app.font(font_weight)
  text_size = measure_text_cached(font, text, font_size)
  display_text = text

  # Elide text to fit within the rectangle
  if elide_right and text_size.x > rect.width:
    _ellipsis = "..."
    left, right = 0, len(text)
    while left < right:
      mid = (left + right) // 2
      candidate = text[:mid] + _ellipsis
      candidate_size = measure_text_cached(font, candidate, font_size)
      if candidate_size.x <= rect.width:
        left = mid + 1
      else:
        right = mid
    display_text = text[: left - 1] + _ellipsis if left > 0 else _ellipsis
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
    (rl.GuiControl.DEFAULT, rl.GuiDefaultProperty.TEXT_WRAP_MODE, rl.GuiTextWrapMode.TEXT_WRAP_WORD)
  ]
  if font_weight != FontWeight.NORMAL:
    rl.gui_set_font(gui_app.font(font_weight))

  with GuiStyleContext(styles):
    rl.gui_label(rect, text)

  if font_weight != FontWeight.NORMAL:
    rl.gui_set_font(gui_app.font(FontWeight.NORMAL))

@dataclass
class RenderElement:
  x: float
  y: float
  texture: Union[rl.Texture, None]  # noqa: UP007
  text: str
  width: float


class Label(Widget):
  """
  label widget

  Supports:
  - Emoji rendering
  - Text wrapping
  - Automatic eliding (single-line or multiline)
  - Proper multiline vertical alignment
  - Height calculation for layout purposes
  """
  def __init__(self,
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
               icon: Union[rl.Texture, None] = None,
               scroll: bool = False):
    super().__init__()
    self._text = text
    self._font_size = font_size
    self._font_weight = font_weight
    self._font = gui_app.font(self._font_weight)
    self._text_color = text_color
    self._alignment = alignment
    self._alignment_vertical = alignment_vertical
    self._text_padding = text_padding
    self._max_width = max_width
    self._elide = elide
    self._wrap_text = wrap_text
    self._line_height = line_height * 0.9
    self._letter_spacing = letter_spacing  # 0.1 = 10%
    self._spacing_pixels = font_size * letter_spacing
    self._icon = icon
    self._scroll = scroll

    # Scroll state
    self._needs_scroll = False
    self._scroll_offset = 0.0
    self._scroll_pause_t: float | None = None
    self._scroll_state: ScrollState = ScrollState.STARTING

    # Cached data
    self._elements: list[RenderElement] = []
    self._cached_text: str | None = None
    self._cached_width: int = -1
    self._cached_height: float = 0.0
    self._cached_content_width: float = 0.0  # text + icon

    # If max_width is set, initialize rect size for Scroller support
    if max_width:
      self._rect.width = max_width
      self._rect.height = self.get_content_height(max_width)

  def set_text(self, text: str | Callable[[], str]):
    """Update the text content."""
    self._text = text
    # No need to update cache here, will be done on next render if needed

  @property
  def text(self) -> str:
    """Get the current text content."""
    return str(_resolve_value(self._text))

  def _invalidate_cache(self):
    self._cached_text = None
    self._cached_width = -1

  def set_text_color(self, color: rl.Color):
    """Update the text color."""
    self._text_color = color

  def set_color(self, color: rl.Color):
    """Update the text color (alias for set_text_color)."""
    self.set_text_color(color)

  def set_font_size(self, size: int):
    """Update the font size."""
    if self._font_size != size:
      self._font_size = size
      self._spacing_pixels = size * self._letter_spacing  # Recalculate spacing
      self._invalidate_cache()

  def set_letter_spacing(self, letter_spacing: float):
    """Update letter spacing (as percentage, e.g., 0.1 = 10%)."""
    if self._letter_spacing != letter_spacing:
      self._letter_spacing = letter_spacing
      self._spacing_pixels = self._font_size * letter_spacing
      self._invalidate_cache()

  def set_font_weight(self, font_weight: FontWeight):
    """Update the font weight."""
    if self._font_weight != font_weight:
      self._font_weight = font_weight
      self._font = gui_app.font(self._font_weight)
      self._invalidate_cache()

  def set_alignment(self, alignment: int):
    """Update the horizontal text alignment."""
    self._alignment = alignment
    self._invalidate_cache()

  def set_alignment_vertical(self, alignment_vertical: int):
    """Update the vertical text alignment."""
    self._alignment_vertical = alignment_vertical

  def set_max_width(self, max_width: int | None):
    """Set the maximum width constraint for wrapping/eliding."""
    if self._max_width != max_width:
      self._max_width = max_width
      self._invalidate_cache()
      # Update rect size for Scroller support
      if max_width is not None:
        self._rect.width = max_width
        self._rect.height = self.get_content_height(max_width)

  def _update_text_cache(self, available_width: int):
    """Update cached text processing data."""
    text = self.text
    if (self._cached_text == text and self._cached_width == available_width and self._elements):
      return  # Cache hit

    self._cached_text = text
    self._cached_width = available_width
    self._elements.clear()

    inner_w = max(1, available_width - self._text_padding * 2)
    icon_w = (self._icon.width + ICON_PADDING) if self._icon else 0
    text_area_w = inner_w - icon_w

     # Wrap or split
    if self._wrap_text and not self._scroll:
      lines = wrap_text(self._font, text, self._font_size, text_area_w, self._spacing_pixels)
    else:
      lines = text.split("\n") if text else [""]

    # Elide if enabled and not scrolling
    if self._elide and not self._scroll:
      lines = [self._elide_line(line, text_area_w) for line in lines]

    # For scrolling, check if single line needs scroll
    if self._scroll and len(lines) == 1:
      single_line = lines[0]
      text_size = measure_text_cached(self._font, single_line, self._font_size, self._spacing_pixels)
      self._needs_scroll = text_size.x > text_area_w
      if self._needs_scroll:
        lines = [single_line]  # Keep as single line for scrolling
      else:
        self._needs_scroll = False

    # Measure
    line_widths = [
        measure_text_cached(self._font, line, self._font_size, self._spacing_pixels).x
        if line else 0
        for line in lines
    ]
    max_text_w = max(line_widths, default=0)
    total_w = max_text_w + icon_w

    # Horizontal block alignment
    if self._alignment == rl.GuiTextAlignment.TEXT_ALIGN_LEFT:
        block_x = self._text_padding
    elif self._alignment == rl.GuiTextAlignment.TEXT_ALIGN_CENTER:
        block_x = (available_width - total_w) / 2
    else:
        block_x = available_width - total_w - self._text_padding

    icon_x = block_x
    text_x = block_x + icon_w

    # Build elements
    y = 0.0
    line_h = self._font_size * self._line_height

    for line, w in zip(lines, line_widths):
        offset = 0.0
        if self._alignment == rl.GuiTextAlignment.TEXT_ALIGN_CENTER:
            offset = (max_text_w - w) / 2
        elif self._alignment == rl.GuiTextAlignment.TEXT_ALIGN_RIGHT:
            offset = max_text_w - w

        elems, _ = self._parse_line(line, text_x + offset, y)
        self._elements.extend(elems)
        y += line_h

    self._cached_height = y
    self._cached_content_width = total_w + self._text_padding * 2

    # Cache icon position for render
    self._cached_icon_x = icon_x


  def _parse_line(self, line_text: str, base_x: float, y: float) -> tuple[list[RenderElement], float]:
    elements, emojis = [], find_emoji(line_text)
    x = base_x

    if not emojis:
      w = measure_text_cached(self._font, line_text, self._font_size).x
      return [RenderElement(x, y, None, line_text, w)], w

    prev = 0
    for start, end, emoji in emojis:
      if start > prev:
        seg = line_text[prev:start]
        w = measure_text_cached(self._font, seg, self._font_size).x
        elements.append(RenderElement(x, y, None, seg, w))
        x += w

      emoji_w = self._font_size * FONT_SCALE
      elements.append(RenderElement(x, y, emoji_tex(emoji), emoji, emoji_w))
      x += emoji_w
      prev = end

    if prev < len(line_text):
      rem = line_text[prev:]
      w = measure_text_cached(self._font, rem, self._font_size).x
      elements.append(RenderElement(x, y, None, rem, w))
      x += w

    return elements, x - base_x

  def _elide_line(self, line: str, max_width: int, force: bool = False) -> str:
    """Elide a single line if it exceeds max_width. If force is True, always elide even if it fits."""
    if not self._elide and not force:
      return line

    text_size = measure_text_cached(self._font, line, self._font_size, self._spacing_pixels)
    if text_size.x <= max_width and not force:
      return line

    ellipsis = "..."
    # If force=True and line fits, just append ellipsis without truncating
    if force and text_size.x <= max_width:
      ellipsis_size = measure_text_cached(self._font, ellipsis, self._font_size, self._spacing_pixels)
      if text_size.x + ellipsis_size.x <= max_width:
        return line + ellipsis
      # If line + ellipsis doesn't fit, need to truncate
      # Fall through to binary search below

    left, right = 0, len(line)
    while left < right:
      mid = (left + right) // 2
      candidate = line[:mid] + ellipsis
      candidate_size = measure_text_cached(self._font, candidate, self._font_size, self._spacing_pixels)
      if candidate_size.x <= max_width:
        left = mid + 1
      else:
        right = mid
    return line[:left - 1] + ellipsis if left > 0 else ellipsis

  def get_content_height(self, max_width: int) -> float:
    """
    Returns the height needed for text at given max_width.
    Similar to HtmlRenderer.get_total_height().
    """
    # Use max_width if provided, otherwise use self._max_width or a default
    w = max_width or self._max_width or 1000
    self._update_text_cache(w)
    return self._cached_height

  def _render(self, rect: rl.Rectangle):
    if rect.width <= 0 or rect.height <= 0:
      return

    width = min(rect.width, self._max_width or rect.width)
    self._update_text_cache(int(width))

    # Vertical alignment
    text_y = rect.y
    if self._alignment_vertical == rl.GuiTextAlignmentVertical.TEXT_ALIGN_MIDDLE:
      text_y += (rect.height - self._cached_height) / 2
    elif self._alignment_vertical == rl.GuiTextAlignmentVertical.TEXT_ALIGN_BOTTOM:
      text_y += rect.height - self._cached_height

    # Icon (vertically centered)
    if self._icon:
      icon_y = rect.y + (rect.height - self._icon.height) / 2
      icon_x = rect.x + self._cached_icon_x
      rl.draw_texture_v(self._icon, rl.Vector2(icon_x, icon_y), rl.WHITE)

    # Scissor for scrolling
    if self._needs_scroll:
      rl.begin_scissor_mode(int(rect.x), int(rect.y), int(rect.width), int(rect.height))

    # Handle scroll state
    scroll_offset = 0.0
    if self._needs_scroll:
      if self._scroll_state == ScrollState.STARTING:
        if self._scroll_pause_t is None:
          self._scroll_pause_t = rl.get_time() + 2.0
        if rl.get_time() >= self._scroll_pause_t:
          self._scroll_state = ScrollState.SCROLLING
          self._scroll_pause_t = None
      elif self._scroll_state == ScrollState.SCROLLING:
        self._scroll_offset -= 0.8 / 60. * gui_app.target_fps
        # Don't fully hide
        text_size = measure_text_cached(self._font, self.text, self._font_size, self._spacing_pixels)
        if self._scroll_offset <= -text_size.x - rect.width / 3:
          self._scroll_offset = 0
          self._scroll_state = ScrollState.STARTING
          self._scroll_pause_t = None
      scroll_offset = self._scroll_offset

    # Text elements
    for el in self._elements:
      pos = rl.Vector2(rect.x + el.x + scroll_offset, text_y + el.y)
      if el.texture:
        scale = self._font_size / el.texture.height * FONT_SCALE
        rl.draw_texture_ex(el.texture, pos, 0.0, scale, self._text_color)
      else:
        rl.draw_text_ex(self._font, el.text, pos, self._font_size, self._spacing_pixels, self._text_color)

    # Draw second instance for seamless scroll
    if self._needs_scroll and self._scroll_state != ScrollState.STARTING:
      text_size = measure_text_cached(self._font, self.text, self._font_size, self._spacing_pixels)
      second_offset = text_size.x + rect.width / 3
      for el in self._elements:
        pos = rl.Vector2(rect.x + el.x + scroll_offset + second_offset, text_y + el.y)
        if el.texture:
          scale = self._font_size / el.texture.height * FONT_SCALE
          rl.draw_texture_ex(el.texture, pos, 0.0, scale, self._text_color)
        else:
          rl.draw_text_ex(self._font, el.text, pos, self._font_size, self._spacing_pixels, self._text_color)

    if self._needs_scroll:
      # Draw fade gradients
      fade_width = 20
      rl.draw_rectangle_gradient_h(int(rect.x + rect.width - fade_width), int(rect.y), fade_width, int(rect.height), rl.Color(0, 0, 0, 0), rl.BLACK)
      if self._scroll_state != ScrollState.STARTING:
        rl.draw_rectangle_gradient_h(int(rect.x), int(rect.y), fade_width, int(rect.height), rl.BLACK, rl.Color(0, 0, 0, 0))
      rl.end_scissor_mode()
