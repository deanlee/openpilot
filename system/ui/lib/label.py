import pyray as rl
from openpilot.system.ui.lib.application import gui_app, FontWeight, DEFAULT_TEXT_SIZE, DEFAULT_TEXT_COLOR
from openpilot.system.ui.lib.utils import GuiStyleContext


def gui_label(
  rect: rl.Rectangle,
  text: str,
  font_size: int = DEFAULT_TEXT_SIZE,
  color: rl.Color = DEFAULT_TEXT_COLOR,
  font_weight: FontWeight = FontWeight.NORMAL,
  alignment: rl.GuiTextAlignment = rl.GuiTextAlignment.TEXT_ALIGN_LEFT,
  alignment_vertical: rl.GuiTextAlignmentVertical = rl.GuiTextAlignmentVertical.TEXT_ALIGN_MIDDLE,
):
  # Set font based on the provided weight
  font = gui_app.font(font_weight)

  # Measure text size
  text_size = rl.measure_text_ex(font, text, font_size, 0)

  # Calculate horizontal position based on alignment
  if alignment == rl.GuiTextAlignment.TEXT_ALIGN_LEFT:
    text_x = rect.x
  elif alignment == rl.GuiTextAlignment.TEXT_ALIGN_CENTER:
    text_x = rect.x + (rect.width - text_size.x) // 2
  elif alignment == rl.GuiTextAlignment.TEXT_ALIGN_RIGHT:
    text_x = rect.x + rect.width - text_size.x

  # Calculate vertical position based on alignment
  if alignment_vertical == rl.GuiTextAlignmentVertical.TEXT_ALIGN_TOP:
    text_y = rect.y
  elif alignment_vertical == rl.GuiTextAlignmentVertical.TEXT_ALIGN_MIDDLE:
    text_y = rect.y + (rect.height - text_size.y) // 2
  elif alignment_vertical == rl.GuiTextAlignmentVertical.TEXT_ALIGN_BOTTOM:
    text_y = rect.y + rect.height - text_size.y

  # Create a position vector for the text
  text_pos = rl.Vector2(text_x, text_y)

  # Draw the text in the specified rectangle
  rl.draw_text_ex(font, text, text_pos, font_size, 0, color)


def gui_text_box(
  rect: rl.Rectangle,
  text: str,
  font_size: int = DEFAULT_TEXT_SIZE,
  color: rl.Color = DEFAULT_TEXT_COLOR,
  alignment: rl.GuiTextAlignment = rl.GuiTextAlignment.TEXT_ALIGN_LEFT,
  alignment_vertical: rl.GuiTextAlignmentVertical = rl.GuiTextAlignmentVertical.TEXT_ALIGN_TOP,
):
  styles = [
    (rl.GuiControl.DEFAULT, rl.GuiControlProperty.TEXT_COLOR_NORMAL, rl.color_to_int(color)),
    (rl.GuiControl.DEFAULT, rl.GuiDefaultProperty.TEXT_SIZE, font_size),
    (rl.GuiControl.DEFAULT, rl.GuiDefaultProperty.TEXT_LINE_SPACING, font_size),
    (rl.GuiControl.DEFAULT, rl.GuiControlProperty.TEXT_ALIGNMENT, alignment),
    (rl.GuiControl.DEFAULT, rl.GuiDefaultProperty.TEXT_ALIGNMENT_VERTICAL, alignment_vertical),
    (rl.GuiControl.DEFAULT, rl.GuiDefaultProperty.TEXT_WRAP_MODE, rl.GuiTextWrapMode.TEXT_WRAP_WORD)
  ]

  with GuiStyleContext(styles):
    rl.gui_label(rect, text)
