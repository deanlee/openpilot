import pyray as rl
from dataclasses import dataclass
from enum import IntEnum
from collections.abc import Callable

from openpilot.common.params import Params
from openpilot.system.ui.widgets.scroller import Scroller
from openpilot.selfdrive.ui.mici.widgets.button import BigButton
from openpilot.selfdrive.ui.mici.layouts.settings.toggles import TogglesLayoutMici
from openpilot.selfdrive.ui.mici.layouts.settings.network import NetworkLayoutMici
from openpilot.selfdrive.ui.mici.layouts.settings.device import DeviceLayoutMici, PairBigButton
from openpilot.selfdrive.ui.mici.layouts.settings.developer import DeveloperLayoutMici
from openpilot.selfdrive.ui.mici.layouts.settings.firehose import FirehoseLayout
from openpilot.system.ui.lib.application import gui_app, FontWeight
from openpilot.system.ui.widgets import Widget, NavWidget


class PanelType(IntEnum):
  TOGGLES = 0
  NETWORK = 1
  DEVICE = 2
  DEVELOPER = 3
  USER_MANUAL = 4
  FIREHOSE = 5


@dataclass
class PanelInfo:
  name: str
  layout: Widget
  icon_path: str
  button_id: str


class SettingsLayout(NavWidget):
  def __init__(self):
    super().__init__()
    self._params = Params()
    self._current_panel = None  # PanelType.DEVICE
    self._close_callback: Callable | None = None

    panels = [
      PanelInfo("Toggles", TogglesLayoutMici(), "icons_mici/settings/toggles_icon.png", "toggles"),
      PanelInfo("Network", NetworkLayoutMici(), "icons_mici/settings/network/wifi_strength_full.png", "network"),
      PanelInfo("Device",   DeviceLayoutMici(),   "icons_mici/settings/device_icon.png", "device"),
      PanelInfo("Firehose", FirehoseLayout(),     "icons_mici/settings/comma_icon.png", "firehose"),
      PanelInfo("Developer", DeveloperLayoutMici(), "icons_mici/settings/developer_icon.png", "developer"),
    ]

    self._panels: dict[PanelType, PanelInfo] = {}
    buttons = []
    for i, panel in enumerate(panels):
      panel_type = PanelType(i)
      self.panel_map[panel_type] = panel

      btn = BigButton(panel.button_id, "", panel.icon_path)
      btn.set_click_callback(lambda pt=panel_type: self._set_current_panel(pt))
      buttons.append(btn)

    buttons.insert(3, PairBigButton())
    self.scroller = Scroller(buttons, snap_items=False)

    # Set up back navigation
    self.set_back_callback(self.close_settings)
    self.set_back_enabled(lambda: self._current_panel is None)

  def show_event(self):
    super().show_event()
    self._set_current_panel(None)
    self._scroller.show_event()
    if self._current_panel is not None:
      self._panels[self._current_panel].instance.show_event()

  def hide_event(self):
    super().hide_event()
    if self._current_panel is not None:
      self._panels[self._current_panel].instance.hide_event()

  def set_callbacks(self, on_close: Callable):
    self._close_callback = on_close

  def _render(self, rect: rl.Rectangle):
    if self._current_panel is not None:
      self._draw_current_panel()
    else:
      self._scroller.render(rect)

  def _draw_current_panel(self):
    panel = self._panels[self._current_panel]
    panel.instance.render(self._rect)

  def _set_current_panel(self, panel_type: PanelType | None):
    if panel_type != self._current_panel:
      if self._current_panel is not None:
        self._panels[self._current_panel].instance.hide_event()
      self._current_panel = panel_type
      if self._current_panel is not None:
        self._panels[self._current_panel].instance.show_event()

  def close_settings(self):
    if self._close_callback:
      self._close_callback()
