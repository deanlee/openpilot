import pyray as rl
from openpilot.selfdrive.ui.lib.prime_state import PrimeType
from openpilot.selfdrive.ui.widgets.pairing_dialog import PairingDialog
from openpilot.selfdrive.ui.ui_state import ui_state
from openpilot.system.ui.lib.application import gui_app, FontWeight
from openpilot.system.ui.lib.button import gui_button, ButtonStyle
from openpilot.system.ui.lib.label import draw_wrapped_text
from openpilot.system.ui.lib.widget import Widget


class SetupWidget(Widget):
  def __init__(self):
    super().__init__()
    self._spacing = 40
    self._open_settings_callback = None
    self._pairing_dialog: PairingDialog | None = None

  def set_open_settings_callback(self, callback):
    self._open_settings_callback = callback

  def _render(self, rect: rl.Rectangle):
    if ui_state.prime_state.get_type() == PrimeType.UNPAIRED:
      self._render_registration(rect)
    else:
      self._render_firehose_prompt(rect)

  def _render_registration(self, rect: rl.Rectangle):
    """Render registration prompt."""

    rl.draw_rectangle_rounded(rl.Rectangle(rect.x, rect.y, rect.width, 660), 0.02, 20, rl.Color(51, 51, 51, 255))

    x = rect.x + 64
    y = rect.y + 48
    w = rect.width - 128

    # Title
    rl.draw_text_ex(gui_app.font(FontWeight.BOLD), "Finish Setup", rl.Vector2(x, y), 75, 0, rl.WHITE)
    y += 75 + self._spacing

    # Description
    desc = "Pair your device with comma connect (connect.comma.ai) and claim your comma prime offer."
    y += draw_wrapped_text(FontWeight.LIGHT, desc, x, y, w, 50)
    y += self._spacing

    button_rect = rl.Rectangle(x, y + 50, w, 128)
    if gui_button(button_rect, "Pair device", button_style=ButtonStyle.PRIMARY):
      self._show_pairing()

  def _render_firehose_prompt(self, rect: rl.Rectangle):
    """Render firehose prompt widget."""

    rl.draw_rectangle_rounded(rl.Rectangle(rect.x, rect.y, rect.width, 450), 0.02, 20, rl.Color(51, 51, 51, 255))

    # Content margins (56, 40, 56, 40)
    x = rect.x + 56
    y = rect.y + 40
    w = rect.width - 112

    # Title with fire emojis
    title_text = "Firehose Mode"
    rl.draw_text_ex(gui_app.font(FontWeight.MEDIUM), title_text, rl.Vector2(x, y), 64, 0, rl.WHITE)
    y += 64 + self._spacing

    # Description
    description = "Maximize your training data uploads to improve openpilot's driving models."
    y += draw_wrapped_text(FontWeight.NORMAL, description, x, y, w, 40)
    y += self._spacing

    # Open button
    button_height = 48 + 64  # font size + padding
    button_rect = rl.Rectangle(x, y, w, button_height)
    if gui_button(button_rect, "Open", button_style=ButtonStyle.PRIMARY):
      if self._open_settings_callback:
        self._open_settings_callback()

  def _show_pairing(self):
    if not self._pairing_dialog:
      self._pairing_dialog = PairingDialog()
    gui_app.set_modal_overlay(self._pairing_dialog, lambda result: setattr(self, '_pairing_dialog', None))

  def __del__(self):
    if self._pairing_dialog:
      del self._pairing_dialog
