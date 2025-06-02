import pyray as rl
from dataclasses import dataclass
from openpilot.system.ui.widgets.network import WifiManagerUI
from openpilot.system.ui.lib.wifi_manager import WifiManagerWrapper


@dataclass
class Setting:
  layout = (None,)
  name: str
  button_rect: rl.Rectangle


class SettingsLayout:
  def __init__(self, window):
    self.window = window
    self.buttons = []
    self._init_buttons()

    self._settings = [
      Setting(name="Device", button_rect=rl.Rectangle(100, 100, 200, 50)),
      Setting(name="Network", button_rect=rl.Rectangle(100, 200, 200, 50)),
      Setting(name="Toggles", button_rect=rl.Rectangle(100, 300, 200, 50)),
      Setting(name="Software", button_rect=rl.Rectangle(100, 300, 200, 50)),
      Setting(name="Firehose", button_rect=rl.Rectangle(100, 300, 200, 50)),
      Setting(name="Developer", button_rect=rl.Rectangle(100, 300, 200, 50)),
      # Add more settings as needed
    ]
    wifi_manager = WifiManagerWrapper()
    self._network_widget = WifiManagerUI(wifi_manager)

  def render(self, rect: rl.Rectangle):
    # Draw side bar
    for setting in self._settings:
      rl.draw_rectangle_rec(setting.button_rect, rl.DARKGRAY)
      rl.draw_text(setting.name, int(setting.button_rect.x + 10), int(setting.button_rect.y + 10), 20, rl.WHITE)
