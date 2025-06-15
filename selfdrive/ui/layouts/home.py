import time
import pyray as rl
from collections.abc import Callable
from enum import IntEnum
from openpilot.common.params import Params
from openpilot.selfdrive.ui.widgets.offroad_alerts import UpdateAlert, OffroadAlert
from openpilot.selfdrive.ui.widgets.exp_mode_button import ExperimentalModeButton
from openpilot.selfdrive.ui.widgets.prime import PrimeWidget
from openpilot.selfdrive.ui.widgets.setup import SetupWidget
from openpilot.system.ui.lib.label import draw_text_center, draw_text_right
from openpilot.system.ui.lib.application import FontWeight, DEFAULT_TEXT_COLOR
from openpilot.system.ui.lib.widget import Widget

HEADER_HEIGHT = 80
HEAD_BUTTON_WIDTH = 200
HEAD_BUTTON_FONT_SIZE = 40
MARGIN = 40
SPACING = 25
RIGHT_COLUMN_WIDTH = 750
REFRESH_INTERVAL = 10.0
PRIME_BG_COLOR = rl.Color(51, 51, 51, 255)


class State(IntEnum):
  HOME = 0
  UPDATE = 1
  ALERTS = 2


class HomeLayout(Widget):
  def __init__(self):
    super().__init__()
    self.params = Params()

    self.update_alert = UpdateAlert()
    self.offroad_alert = OffroadAlert()
    self._prime_widget = PrimeWidget()
    self._setup_widget = SetupWidget()
    self._exp_mode_button = ExperimentalModeButton()


    self.state = State.HOME
    self.last_refresh = 0.0
    self.settings_callback: callable | None = None

    self.update_available = False
    self.alert_count = 0

    self.header_rect = rl.Rectangle(0, 0, 0, 0)
    self.content_rect = rl.Rectangle(0, 0, 0, 0)

    self.update_notif_rect = rl.Rectangle(0, 0, 200, HEADER_HEIGHT)
    self.alert_notif_rect = rl.Rectangle(0, 0, 220, HEADER_HEIGHT)

    self._setup_callbacks()

  def _setup_callbacks(self):
    self.update_alert.set_dismiss_callback(lambda: self._set_state(State.HOME))
    self.offroad_alert.set_dismiss_callback(lambda: self._set_state(State.HOME))

  def set_settings_callback(self, callback: Callable):
    self.settings_callback = callback

  def _set_state(self, state: State):
    self.state = state

  def _render(self, rect: rl.Rectangle):
    current_time = time.time()
    if current_time - self.last_refresh >= REFRESH_INTERVAL:
      self._refresh()
      self.last_refresh = current_time

    self._handle_input()
    self._render_header()

    # Render content based on current state
    if self.state == State.HOME:
      self._render_home_content(self.content_rect)
    elif self.state == State.UPDATE:
      self.update_alert.render(self.content_rect)
    elif self.state == State.ALERTS:
      self.offroad_alert.render(self.content_rect)

  def _update_layout_rects(self):
    self.header_rect = rl.Rectangle(self._rect.x + MARGIN, self._rect.y + MARGIN, self._rect.width - 2 * MARGIN, HEADER_HEIGHT)
    self.content_rect = rl.Rectangle(
      self._rect.x + MARGIN, self._rect.y + MARGIN + HEADER_HEIGHT + SPACING,
      self._rect.width - 2 * MARGIN, self._rect.height - HEADER_HEIGHT - SPACING - 2 * MARGIN
    )

    self.update_notif_rect.x = self.header_rect.x
    self.update_notif_rect.y = self.header_rect.y

    notif_x = self.header_rect.x + (240 if self.update_available else 0)
    self.alert_notif_rect.x = notif_x
    self.alert_notif_rect.y = self.header_rect.y

  def _handle_input(self):
    if not rl.is_mouse_button_pressed(rl.MouseButton.MOUSE_BUTTON_LEFT):
      return

    mouse_pos = rl.get_mouse_position()

    if self.update_available and rl.check_collision_point_rec(mouse_pos, self.update_notif_rect):
      self._set_state(State.UPDATE)
      return

    if self.alert_count > 0 and rl.check_collision_point_rec(mouse_pos, self.alert_notif_rect):
      self._set_state(State.ALERTS)
      return

    # Content area input handling
    if self.state == State.UPDATE:
      self.update_alert.handle_input(mouse_pos, True)
    elif self.state == State.ALERTS:
      self.offroad_alert.handle_input(mouse_pos, True)

  def _render_header(self):
    # Update notification button
    if self.update_available:
      highlight_color = rl.Color(255, 140, 40, 255) if self.state == State.UPDATE else rl.Color(255, 102, 0, 255)
      rl.draw_rectangle_rounded(self.update_notif_rect, 0.3, 10, highlight_color)
      draw_text_center(FontWeight.MEDIUM, "UPDATE", self.update_notif_rect, HEAD_BUTTON_FONT_SIZE, rl.WHITE)

    # Alert notification button
    if self.alert_count > 0:
      highlight_color = rl.Color(255, 70, 70, 255) if self.state == State.ALERTS else rl.Color(226, 44, 44, 255)
      rl.draw_rectangle_rounded(self.alert_notif_rect, 0.3, 10, highlight_color)

      alert_text = f"{self.alert_count} ALERT{'S' if self.alert_count > 1 else ''}"
      draw_text_center(FontWeight.MEDIUM, alert_text, self.alert_notif_rect, HEAD_BUTTON_FONT_SIZE, rl.WHITE)

    # Version text (right aligned)
    draw_text_right(FontWeight.NORMAL, self._get_version_text(), self.header_rect, 48, DEFAULT_TEXT_COLOR)

  def _render_home_content(self, rect):
    left_width = rect.width - RIGHT_COLUMN_WIDTH - SPACING
    left_rect = rl.Rectangle(rect.x, rect.y, left_width, rect.height)
    right_rect = rl.Rectangle(rect.x + left_width + SPACING, rect.y, RIGHT_COLUMN_WIDTH, rect.height)

    self._prime_widget.render(left_rect)
    self._render_right_column(right_rect)

  def _render_right_column(self, rect):
    exp_height = 125
    rect = rl.Rectangle(rect.x, rect.y, rect.width, exp_height)
    self._exp_mode_button.render(rect)

    rect.y += exp_height + SPACING
    rect.height -= exp_height + SPACING
    self._setup_widget.render(rect)

  def _refresh(self):
    self.update_available = self.update_alert.refresh()
    self.alert_count = self.offroad_alert.refresh()

    if self.state == State.HOME:
      if self.update_available:
        self.state = State.UPDATE
      elif self.alert_count > 0:
        self.state = State.ALERTS

  def _get_version_text(self) -> str:
    brand = "openpilot"
    description = self.params.get("UpdaterCurrentDescription", encoding='utf-8')
    return f"{brand} {description}" if description else brand
