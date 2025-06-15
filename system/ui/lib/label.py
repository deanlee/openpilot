import pyray as rl
from openpilot.system.ui.lib.application import gui_app, FontWeight, DEFAULT_TEXT_SIZE, DEFAULT_TEXT_COLOR
from openpilot.system.ui.lib.text_measure import measure_text_cached
from openpilot.system.ui.lib.utils import GuiStyleContext


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
    ellipsis = "..."
    left, right = 0, len(text)
    while left < right:
      mid = (left + right) // 2
      candidate = text[:mid] + ellipsis
      candidate_size = measure_text_cached(font, candidate, font_size)
      if candidate_size.x <= rect.width:
        left = mid + 1
      else:
        right = mid
    display_text = text[: left - 1] + ellipsis if left > 0 else ellipsis
    text_size = measure_text_cached(font, display_text, font_size)

  # Calculate horizontal position based on alignment
  if alignment == rl.GuiTextAlignment.TEXT_ALIGN_LEFT:
    text_x = rect.x
  elif alignment == rl.GuiTextAlignment.TEXT_ALIGN_CENTER:
    text_x = rect.x + (rect.width - text_size.x) / 2
  elif alignment == rl.GuiTextAlignment.TEXT_ALIGN_RIGHT:
    text_x = rect.x + rect.width - text_size.x

  if alignment_vertical == rl.GuiTextAlignmentVertical.TEXT_ALIGN_TOP:
    text_y = rect.y
  elif alignment_vertical == rl.GuiTextAlignmentVertical.TEXT_ALIGN_MIDDLE:
    text_y = rect.y + (rect.height - text_size.y) / 2
  elif alignment_vertical == rl.GuiTextAlignmentVertical.TEXT_ALIGN_BOTTOM:
    text_y = rect.y + rect.height - text_size.y

  # Draw the text in the specified rectangle
  rl.draw_text_ex(font, display_text, rl.Vector2(text_x, text_y), font_size, 0, color)


def draw_text_left(font_weight: FontWeight, text: str, rect: rl.Rectangle, font_size: int, color: rl.Color):
  gui_label(rect, text,font_size, color, font_weight=font_weight)

def draw_text_center(font_weight: FontWeight, text: str, rect: rl.Rectangle, font_size: int, color: rl.Color):
  gui_label(rect, text,font_size, color, font_weight=font_weight, alignment=rl.GuiTextAlignment.TEXT_ALIGN_CENTER,
             alignment_vertical=rl.GuiTextAlignmentVertical.TEXT_ALIGN_MIDDLE)


def draw_text_right(font_weight: FontWeight, text: str, rect: rl.Rectangle, font_size: int, color: rl.Color):
  gui_label(rect, text, font_size, color, font_weight=font_weight, alignment=rl.GuiTextAlignment.TEXT_ALIGN_RIGHT,
            alignment_vertical=rl.GuiTextAlignmentVertical.TEXT_ALIGN_MIDDLE)

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
    (rl.GuiControl.DEFAULT, rl.GuiDefaultProperty.TEXT_SIZE, font_size),
    (rl.GuiControl.DEFAULT, rl.GuiDefaultProperty.TEXT_LINE_SPACING, font_size),
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
