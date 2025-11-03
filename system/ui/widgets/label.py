from collections.abc import Callable
from dataclasses import dataclass
from typing import Union
import pyray as rl

from openpilot.system.ui.lib.application import gui_app, FontWeight, DEFAULT_TEXT_SIZE, DEFAULT_TEXT_COLOR, FONT_SCALE
from openpilot.system.ui.lib.text_measure import measure_text_cached
from openpilot.system.ui.lib.utils import GuiStyleContext
from openpilot.system.ui.lib.emoji import find_emoji, emoji_tex
from openpilot.system.ui.lib.wrap_text import wrap_text
from openpilot.system.ui.widgets import Widget

ICON_PADDING = 15


# TODO: make this common
def _resolve_value(value, default=""):
  if callable(value):
    return value()
  return value if value is not None else default

@dataclass
class RenderElement:
  """Pre-positioned element (text or texture) ready for rendering."""
  x: float
  y: float
  texture: Union[rl.Texture, None]  # noqa: UP007
  # texture: Optional[rl.Texture]  # None = text, not None = emoji/icon
  text: str
  width: float


@dataclass
class LayoutCache:
  """Cached layout for fast rendering."""
  text: str
  rect_key: tuple[float, float, float, float]  # x, y, width, height
  elements: list[RenderElement]


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
  rl.draw_text_ex(font, display_text, rl.Vector2(text_x, text_y), font_size, 0, color)


def gui_text_box(
  rect: rl.Rectangle,
  text: str,
  font_size: int = DEFAULT_TEXT_SIZE,
  color: rl.Color = DEFAULT_TEXT_COLOR,
  alignment: int = rl.GuiTextAlignment.TEXT_ALIGN_LEFT,
  alignment_vertical: int = rl.GuiTextAlignmentVertical.TEXT_ALIGN_TOP,
  font_weight: FontWeight = FontWeight.NORMAL,
):
  styles = [
    (rl.GuiControl.DEFAULT, rl.GuiControlProperty.TEXT_COLOR_NORMAL, rl.color_to_int(color)),
    (rl.GuiControl.DEFAULT, rl.GuiDefaultProperty.TEXT_SIZE, round(font_size * FONT_SCALE)),
    (rl.GuiControl.DEFAULT, rl.GuiDefaultProperty.TEXT_LINE_SPACING, round(font_size * FONT_SCALE)),
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


# Non-interactive text area. Can render emojis and an optional specified icon.
class Label(Widget):
  def __init__(self,
               text: str | Callable[[], str],
               font_size: int = DEFAULT_TEXT_SIZE,
               font_weight: FontWeight = FontWeight.NORMAL,
               text_alignment: int = rl.GuiTextAlignment.TEXT_ALIGN_CENTER,
               text_alignment_vertical: int = rl.GuiTextAlignmentVertical.TEXT_ALIGN_MIDDLE,
               text_padding: int = 0,
               text_color: rl.Color = DEFAULT_TEXT_COLOR,
               icon: Union[rl.Texture, None] = None,  # noqa: UP007
               elide_right: bool = False,
               ):

    super().__init__()
    self._text = text
    self._font_weight = font_weight
    self._font = gui_app.font(self._font_weight)
    self._font_size = font_size
    self._text_alignment = text_alignment
    self._text_alignment_vertical = text_alignment_vertical
    self._text_padding = text_padding
    self._text_color = text_color
    self._icon = icon
    self._elide_right = elide_right
    self._layout_cache: LayoutCache | None = None

  def set_text(self, text):
    self._text = text
    self._layout_cache = None

  def set_text_color(self, color):
    self._text_color = color

  def set_font_size(self, size):
    self._font_size = size
    self._layout_cache = None

  def _calculate_layout(self, text: str) -> LayoutCache:
    """Pre-calculate all element positions (icon, text segments, emojis)."""
    elements: list[RenderElement] = []
    rect_key = (self._rect.x, self._rect.y, self._rect.width, self._rect.height)

    if not text and not self._icon:
      return LayoutCache(text, rect_key, elements)

    content_width = self._rect.width - self._text_padding * 2

    # Wrap or elide text
    if self._elide_right:
      text_size = measure_text_cached(self._font, text, self._font_size)

      if text_size.x > content_width:
        ellipsis = "..."
        left, right = 0, len(text)
        while left < right:
          mid = (left + right) // 2
          candidate = text[:mid] + ellipsis
          candidate_size = measure_text_cached(self._font, candidate, self._font_size)
          if candidate_size.x <= content_width:
            left = mid + 1
          else:
            right = mid
        display_text = text[:left - 1] + ellipsis if left > 0 else ellipsis
      else:
        display_text = text

      wrapped_lines = [display_text] if display_text else []
    else:
      wrapped_lines = wrap_text(self._font, text, self._font_size, int(content_width)) if text else []

    # Calculate starting Y position
    if wrapped_lines:
      first_line_height = measure_text_cached(self._font, wrapped_lines[0], self._font_size).y
    elif self._icon:
      first_line_height = self._icon.height
    else:
      return LayoutCache(text, rect_key, elements)

    if self._text_alignment_vertical == rl.GuiTextAlignmentVertical.TEXT_ALIGN_MIDDLE:
      current_y = self._rect.y + (self._rect.height - first_line_height) / 2
    else:
      current_y = self._rect.y

    # Process first line with optional icon
    if wrapped_lines or self._icon:
      line_elements = []
      line_width = 0.0

      # Add icon as first element
      if self._icon:
        icon_y = current_y + (first_line_height - self._icon.height) / 2
        line_elements.append(RenderElement(
          x=0.0,  # Relative, will be adjusted
          y=icon_y,
          texture=self._icon,
          text="",
          width=self._icon.width
        ))
        line_width += self._icon.width + ICON_PADDING

      # Parse first line (text + emojis)
      if wrapped_lines:
        line_text = wrapped_lines[0]
        line_elems, text_width = self._parse_line_elements(line_text, current_y)
        line_elements.extend(line_elems)
        line_width += text_width

      # Apply horizontal alignment
      self._apply_alignment(line_elements, line_width)
      elements.extend(line_elements)
      current_y += first_line_height

    # Process remaining lines (no icon)
    for line_text in wrapped_lines[1:]:
      line_elems, line_width = self._parse_line_elements(line_text, current_y)
      self._apply_alignment(line_elems, line_width)
      elements.extend(line_elems)
      current_y += self._font_size * FONT_SCALE

    return LayoutCache(text, rect_key, elements)

  def _parse_line_elements(self, line_text: str, y: float) -> tuple[list[RenderElement], float]:
    """Parse line into text and emoji elements with relative X positions."""
    elements = []
    total_width = 0.0
    emojis = find_emoji(line_text)

    if not emojis:
      # Fast path: no emojis
      width = measure_text_cached(self._font, line_text, self._font_size).x
      elements.append(RenderElement(
        x=total_width,
        y=y,
        texture=None,
        text=line_text,
        width=width
      ))
      total_width += width
    else:
      # Parse text and emojis
      prev_idx = 0

      for start, end, emoji in emojis:
        # Text before emoji
        if start > prev_idx:
          text_segment = line_text[prev_idx:start]
          width = measure_text_cached(self._font, text_segment, self._font_size).x
          elements.append(RenderElement(
            x=total_width,
            y=y,
            texture=None,
            text=text_segment,
            width=width
          ))
          total_width += width

        # Emoji
        tex = emoji_tex(emoji)
        emoji_width = self._font_size * FONT_SCALE
        elements.append(RenderElement(
          x=total_width,
          y=y,
          texture=tex,
          text=emoji,
          width=emoji_width
        ))
        total_width += emoji_width
        prev_idx = end

      # Remaining text
      if prev_idx < len(line_text):
        remaining = line_text[prev_idx:]
        width = measure_text_cached(self._font, remaining, self._font_size).x
        elements.append(RenderElement(
          x=total_width,
          y=y,
          texture=None,
          text=remaining,
          width=width
        ))
        total_width += width

    return elements, total_width

  def _apply_alignment(self, elements: list[RenderElement], line_width: float) -> None:
    """Convert relative X positions to absolute based on alignment."""
    if self._text_alignment == rl.GuiTextAlignment.TEXT_ALIGN_LEFT:
      x_base = self._rect.x + self._text_padding
    elif self._text_alignment == rl.GuiTextAlignment.TEXT_ALIGN_CENTER:
      x_base = self._rect.x + (self._rect.width - line_width) / 2
    else:  # RIGHT
      x_base = self._rect.x + self._rect.width - line_width - self._text_padding

    for element in elements:
      element.x += x_base

  def _render(self, _) -> None:
    """Ultra-fast render: single loop over pre-calculated elements."""
    text = _resolve_value(self._text)

    # Update layout cache if needed
    rect_key = (self._rect.x, self._rect.y, self._rect.width, self._rect.height)
    if (self._layout_cache is None or
        self._layout_cache.text != text or
        self._layout_cache.rect_key != rect_key):
      self._layout_cache = self._calculate_layout(text)

    # Fast render loop
    for element in self._layout_cache.elements:
      if element.texture:
        # Draw texture (emoji or icon)
        if element.texture == self._icon:
          # Icon: draw as-is
          rl.draw_texture_v(element.texture, rl.Vector2(element.x, element.y), rl.WHITE)
        else:
          # Emoji: scale to font size
          scale = self._font_size / element.texture.height * FONT_SCALE
          rl.draw_texture_ex(
            element.texture,
            rl.Vector2(element.x, element.y),
            0.0,
            scale,
            self._text_color
          )
      elif element.text:
        # Draw text
        rl.draw_text_ex(
          self._font,
          element.text,
          rl.Vector2(element.x, element.y),
          self._font_size,
          0,
          self._text_color
        )
