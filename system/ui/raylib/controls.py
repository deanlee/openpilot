import pyray as rl

def gui_label(rect, text, font_size):
  prev_font_size = rl.gui_get_style(rl.GuiControl.DEFAULT, rl.GuiDefaultProperty.TEXT_SIZE)
  prev_text_line_spacing = rl.gui_get_style(rl.GuiControl.DEFAULT, rl.GuiDefaultProperty.TEXT_LINE_SPACING)
  rl.gui_set_style(rl.GuiControl.DEFAULT, rl.GuiDefaultProperty.TEXT_SIZE, font_size)
  rl.gui_set_style(rl.GuiControl.DEFAULT, rl.GuiDefaultProperty.TEXT_LINE_SPACING, font_size)
  rl.gui_label(rect, text)
  rl.gui_set_style(rl.GuiControl.DEFAULT, rl.GuiDefaultProperty.TEXT_SIZE, prev_font_size)
  rl.gui_set_style(rl.GuiControl.DEFAULT, rl.GuiDefaultProperty.TEXT_LINE_SPACING, prev_text_line_spacing)

def gui_button(rect, text, bg_color=rl.Color(51, 51, 51, 255)):
  prev_bg_color = rl.gui_get_style(rl.GuiControl.DEFAULT, rl.GuiControlProperty.BASE_COLOR_NORMAL)

  rl.gui_set_style(rl.GuiControl.DEFAULT, rl.GuiDefaultProperty.TEXT_ALIGNMENT_VERTICAL, rl.GuiTextAlignmentVertical.TEXT_ALIGN_MIDDLE)
  rl.gui_set_style(rl.GuiControl.DEFAULT, rl.GuiControlProperty.BASE_COLOR_NORMAL, rl.color_to_int(bg_color))

  pressed = rl.gui_button(rect, text)

  rl.gui_set_style(rl.GuiControl.DEFAULT, rl.GuiControlProperty.BASE_COLOR_NORMAL, prev_bg_color)
  rl.gui_set_style(rl.GuiControl.DEFAULT, rl.GuiDefaultProperty.TEXT_ALIGNMENT_VERTICAL, rl.GuiTextAlignmentVertical.TEXT_ALIGN_TOP)
  return pressed
