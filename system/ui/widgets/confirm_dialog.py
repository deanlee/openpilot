import pyray as rl
from openpilot.system.ui.lib.application import gui_app
from openpilot.system.ui.lib.button import gui_button, ButtonStyle
from openpilot.system.ui.lib.label import gui_text_box
from openpilot.system.ui.lib.layout import HLayout, VLayout, Spacing

DIALOG_WIDTH = 1520
DIALOG_HEIGHT = 600
BUTTON_HEIGHT = 160
MARGIN = 50
TEXT_AREA_HEIGHT_REDUCTION = 200
BACKGROUND_COLOR = rl.Color(27, 27, 27, 255)

def confirm_dialog(message: str, confirm_text: str, cancel_text: str = "Cancel") -> int:
  dialog_x = (gui_app.width - DIALOG_WIDTH) / 2
  dialog_y = (gui_app.height - DIALOG_HEIGHT) / 2
  dialog_rect = rl.Rectangle(dialog_x, dialog_y, DIALOG_WIDTH, DIALOG_HEIGHT)

  rl.draw_rectangle_rec(dialog_rect, BACKGROUND_COLOR)

  # Layout setup
  layout = VLayout(dialog_rect)
  layout.padding = Spacing(MARGIN, MARGIN, MARGIN, MARGIN)
  layout.spacing = 40

  # Text area
  text_item = layout.add_stretch_item()
  # Button row
  button_row = layout.add_layout(HLayout())
  button_row.spacing = MARGIN

  button_width = (DIALOG_WIDTH - 3 * MARGIN) // 2

  cancel_item = button_row.add_stretch_item(button_width, BUTTON_HEIGHT)
  confirm_item = button_row.add_stretch_item(button_width, BUTTON_HEIGHT)

  layout.update_layout()

  # Draw message
  gui_text_box(
    text_item.rect,
    message,
    alignment=rl.GuiTextAlignment.TEXT_ALIGN_CENTER,
    alignment_vertical=rl.GuiTextAlignmentVertical.TEXT_ALIGN_MIDDLE,
  )

  result = -1

  # Keyboard shortcuts
  if rl.is_key_pressed(rl.KeyboardKey.KEY_ENTER):
    result = 1
  elif rl.is_key_pressed(rl.KeyboardKey.KEY_ESCAPE):
    result = 0

  # Buttons
  if gui_button(confirm_item.rect, confirm_text, button_style=ButtonStyle.PRIMARY):
    result = 1
  if gui_button(cancel_item.rect, cancel_text):
    result = 0

  return result
