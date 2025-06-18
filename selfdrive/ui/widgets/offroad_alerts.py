import json
import pyray as rl
from abc import ABC, abstractmethod
from collections.abc import Callable
from dataclasses import dataclass
from openpilot.common.params import Params
from openpilot.system.hardware import HARDWARE
from openpilot.system.ui.lib.button import gui_button, ButtonStyle
from openpilot.system.ui.lib.label import draw_wrapped_text, get_wrapped_text_height
from openpilot.system.ui.lib.scroll_panel import GuiScrollPanel
from openpilot.system.ui.lib.text_measure import measure_text_cached
from openpilot.system.ui.lib.application import gui_app, FontWeight
from openpilot.system.ui.lib.widget import Widget


class AlertColors:
  HIGH_SEVERITY = rl.Color(226, 44, 44, 255)
  LOW_SEVERITY = rl.Color(41, 41, 41, 255)
  BACKGROUND = rl.Color(57, 57, 57, 255)


class AlertConstants:
  BUTTON_HEIGHT = 125
  MARGIN = 50
  SPACING = 30
  FONT_SIZE = 48
  BORDER_RADIUS = 30
  ALERT_SPACING = 20


@dataclass
class AlertData:
  key: str
  text: str
  severity: int
  visible: bool = False


class AbstractAlert(Widget, ABC):
  def __init__(self, has_reboot_btn: bool = False):
    super().__init__()
    self.params = Params()
    self.has_reboot_btn = has_reboot_btn
    self.dismiss_callback: Callable | None = None

    self.snooze_visible = False
    self.content_rect = rl.Rectangle(0, 0, 0, 0)
    self.scroll_panel = GuiScrollPanel()

  def set_dismiss_callback(self, callback: Callable):
    self.dismiss_callback = callback

  @abstractmethod
  def refresh(self) -> bool:
    pass

  @abstractmethod
  def get_content_height(self) -> float:
    pass

  def _render(self, rect: rl.Rectangle):
    rl.draw_rectangle_rounded(rect, AlertConstants.BORDER_RADIUS / rect.height, 10, AlertColors.BACKGROUND)

    footer_height = AlertConstants.BUTTON_HEIGHT + AlertConstants.SPACING
    self.content_rect = rl.Rectangle(rect.x + AlertConstants.MARGIN, rect.y + AlertConstants.MARGIN,
                                     rect.width - 2 * AlertConstants.MARGIN, rect.height - 2 * AlertConstants.MARGIN - footer_height)
    self._render_scrollable_content(self.content_rect)
    self._render_footer(rect)

  def _render_scrollable_content(self, content_rect: rl.Rectangle):
    content_height = self.get_content_height()
    content_bounds = rl.Rectangle(0, 0, content_rect.width, content_height)
    scroll_offset = self.scroll_panel.handle_scroll(content_rect, content_bounds)

    rl.begin_scissor_mode(int(content_rect.x), int(content_rect.y), int(content_rect.width), int(content_rect.height))
    content_rect_with_scroll = rl.Rectangle(
      content_rect.x, content_rect.y + scroll_offset.y, content_rect.width, content_height
    )
    self._render_content(content_rect_with_scroll)
    rl.end_scissor_mode()

  @abstractmethod
  def _render_content(self, content_rect: rl.Rectangle):
    pass

  def _render_footer(self, rect: rl.Rectangle):
    footer_y = rect.y + rect.height - AlertConstants.MARGIN - AlertConstants.BUTTON_HEIGHT

    close_btn = rl.Rectangle(rect.x + AlertConstants.MARGIN, footer_y, 400, AlertConstants.BUTTON_HEIGHT)
    if gui_button(close_btn, "Close", AlertConstants.FONT_SIZE, FontWeight.MEDIUM, ButtonStyle.WHITE):
      if self.dismiss_callback:
        self.dismiss_callback()

    if self.snooze_visible:
      snooze_btn = rl.Rectangle(rect.x + rect.width - AlertConstants.MARGIN - 550, footer_y, 550, AlertConstants.BUTTON_HEIGHT)
      if gui_button(snooze_btn, "Snooze Update", AlertConstants.FONT_SIZE, FontWeight.MEDIUM, ButtonStyle.LIST_ACTION):
        self.params.put_bool("SnoozeUpdate", True)
        if self.dismiss_callback:
          self.dismiss_callback()
    elif self.has_reboot_btn:
      reboot_btn = rl.Rectangle(rect.x + rect.width - AlertConstants.MARGIN - 600, footer_y, 600, AlertConstants.BUTTON_HEIGHT)
      if gui_button(reboot_btn, "Reboot and Update", AlertConstants.FONT_SIZE, FontWeight.MEDIUM, ButtonStyle.WHITE):
        HARDWARE.reboot()


class OffroadAlert(AbstractAlert):
  def __init__(self):
    super().__init__(has_reboot_btn=False)
    self.sorted_alerts: list[AlertData] = []

  def refresh(self):
    if not self.sorted_alerts:
      self._build_alerts()

    active_count = 0
    self.snooze_visible = False

    for alert in self.sorted_alerts:
      try:
        alert_json = json.loads(self.params.get(alert.key) or b'{}')
        alert.text = alert_json.get("text", "").replace("{}", alert_json.get("extra", ""))
      except json.JSONDecodeError:
        alert.text = ""

      alert.visible = bool(alert.text)
      if alert.visible:
        active_count += 1
        if alert.key == "Offroad_ConnectivityNeeded":
          self.snooze_visible = True

    return active_count

  def get_content_height(self) -> float:
    text_width = int(self.content_rect.width - 90)
    heights = [
      get_wrapped_text_height(FontWeight.NORMAL, alert.text, AlertConstants.FONT_SIZE, text_width) + 40
      for alert in self.sorted_alerts
      if alert.visible
    ]
    return sum(heights) + 40 + (len(heights) - 1) * AlertConstants.ALERT_SPACING if heights else 0

  def _build_alerts(self):
    self.sorted_alerts = []
    try:
      with open("../selfdrived/alerts_offroad.json", "rb") as f:
        alerts_config = json.load(f)
        for key, config in sorted(alerts_config.items(), key=lambda x: x[1].get("severity", 0), reverse=True):
          severity = config.get("severity", 0)
          alert_data = AlertData(key=key, text="", severity=severity)
          self.sorted_alerts.append(alert_data)
    except (FileNotFoundError, json.JSONDecodeError):
      pass

  def _render_content(self, content_rect: rl.Rectangle):
    y_offset = 20

    for alert_data in self.sorted_alerts:
      if not alert_data.visible:
        continue

      bg_color = AlertColors.HIGH_SEVERITY if alert_data.severity > 0 else AlertColors.LOW_SEVERITY
      text_width = int(content_rect.width - 90)
      alert_item_height = get_wrapped_text_height(FontWeight.NORMAL, alert_data.text, AlertConstants.FONT_SIZE, text_width) + 40
      alert_rect = rl.Rectangle(content_rect.x + 10, content_rect.y + y_offset, content_rect.width - 30, alert_item_height)
      rl.draw_rectangle_rounded(alert_rect, 0.2, 10, bg_color)

      text_x = alert_rect.x + 30
      text_y = alert_rect.y + 20
      draw_wrapped_text(FontWeight.NORMAL, alert_data.text, text_x, text_y, text_width, AlertConstants.FONT_SIZE, rl.WHITE)
      y_offset += alert_item_height + AlertConstants.ALERT_SPACING


class UpdateAlert(AbstractAlert):
  def __init__(self):
    super().__init__(has_reboot_btn=True)
    self.release_notes = ""

  def refresh(self) -> bool:
    update_available: bool = self.params.get_bool("UpdateAvailable")
    if update_available:
      self.release_notes = self.params.get("UpdaterNewReleaseNotes", encoding='utf-8')
    return update_available

  def get_content_height(self) -> float:
    size = measure_text_cached(gui_app.font(FontWeight.NORMAL), self.release_notes, AlertConstants.FONT_SIZE)
    return max(size.y + 60, 100)

  def _render_content(self, content_rect: rl.Rectangle):
    if self.release_notes:
      rl.draw_text_ex(
        gui_app.font(FontWeight.NORMAL),
        self.release_notes,
        rl.Vector2(content_rect.x + 30, content_rect.y + 30),
        AlertConstants.FONT_SIZE,
        0.0,
        rl.WHITE,
      )
