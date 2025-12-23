import time
import pyray as rl
from collections.abc import Callable
from enum import IntEnum
from openpilot.common.params import Params
from openpilot.selfdrive.ui.widgets.offroad_alerts import UpdateAlert, OffroadAlert
from openpilot.selfdrive.ui.widgets.exp_mode_button import ExperimentalModeButton
from openpilot.selfdrive.ui.widgets.prime import PrimeWidget
from openpilot.selfdrive.ui.widgets.setup import SetupWidget
from openpilot.system.ui.lib.text_measure import measure_text_cached
from openpilot.system.ui.lib.application import gui_app, FontWeight, MousePos
from openpilot.system.ui.lib.multilang import tr, trn
from openpilot.system.ui.widgets.label import gui_label, Align
from openpilot.system.ui.widgets import Widget

HEADER_HEIGHT = 80
HEAD_BUTTON_FONT_SIZE = 40
CONTENT_MARGIN = 40
SPACING = 25
RIGHT_COLUMN_WIDTH = 750
REFRESH_INTERVAL = 10.0


class HomeLayoutState(IntEnum):
  HOME = 0
  UPDATE = 1
  ALERTS = 2


class HomeLayout(Widget):
  def __init__(self):
    super().__init__()
    self.params = Params()

    self._prime_widget = PrimeWidget()
    self._setup_widget = SetupWidget()
    self._exp_mode_button = ExperimentalModeButton()
    self.update_alert = UpdateAlert()
    self.offroad_alert = OffroadAlert()

    self._layout_widgets = {HomeLayoutState.UPDATE: self.update_alert, HomeLayoutState.ALERTS: self.offroad_alert}

    self.current_state = HomeLayoutState.HOME
    self.last_refresh = 0
    self.settings_callback: callable | None = None

    self.update_available = False
    self.alert_count = 0
    self._version_text = ""
    self._prev_update_available = False
    self._prev_alerts_present = False

    self._setup_callbacks()

  def show_event(self):
    self._exp_mode_button.show_event()
    self._refresh()

  def _setup_callbacks(self):
    self.update_alert.set_dismiss_callback(lambda: self._set_state(HomeLayoutState.HOME))
    self.offroad_alert.set_dismiss_callback(lambda: self._set_state(HomeLayoutState.HOME))
    self._exp_mode_button.set_click_callback(lambda: self.settings_callback() if self.settings_callback else None)

  def set_settings_callback(self, callback: Callable):
    self.settings_callback = callback

  def _set_state(self, state: HomeLayoutState):
    # propagate show/hide events
    if state != self.current_state:
      if state == HomeLayoutState.HOME:
        self._exp_mode_button.show_event()

      if state in self._layout_widgets:
        self._layout_widgets[state].show_event()
      if self.current_state in self._layout_widgets:
        self._layout_widgets[self.current_state].hide_event()

    self.current_state = state

  def _render(self, rect: rl.Rectangle):
    current_time = time.monotonic()
    if current_time - self.last_refresh >= REFRESH_INTERVAL:
      self._refresh()

    self._render_header()

    # Render content based on current state
    if self.current_state == HomeLayoutState.HOME:
      self._render_left_column()
      self._render_right_column()
    elif self.current_state == HomeLayoutState.UPDATE:
      self.update_alert.render(self.content_rect)
    elif self.current_state == HomeLayoutState.ALERTS:
      self.offroad_alert.render(self.content_rect)

  def _update_state(self):
    self.header_rect = rl.Rectangle(self._rect.x + CONTENT_MARGIN, self._rect.y + CONTENT_MARGIN,
                                    self._rect.width - 2 * CONTENT_MARGIN, HEADER_HEIGHT)
    self.content_rect = rl.Rectangle(self.header_rect.x, self.header_rect.y + HEADER_HEIGHT + SPACING,
                                     self.header_rect.width, self._rect.height - HEADER_HEIGHT - SPACING - 2*CONTENT_MARGIN)

    l_width = self.content_rect.width - RIGHT_COLUMN_WIDTH - SPACING
    self.left_col_rect = rl.Rectangle(self.content_rect.x, self.content_rect.y, l_width, self.content_rect.height)
    self.right_col_rect = rl.Rectangle(self.content_rect.x + l_width + SPACING, self.content_rect.y,
                                          RIGHT_COLUMN_WIDTH, self.content_rect.height)

    self.update_notif_rect = rl.Rectangle(self.header_rect.x, self.header_rect.y + 10, 200, 60)
    self.alert_notif_rect = rl.Rectangle(self.header_rect.x + (220 if self.update_available else 0),
                                         self.header_rect.y + 10, 220, 60)

  def _handle_mouse_release(self, mouse_pos: MousePos):
    if self.update_available and rl.check_collision_point_rec(mouse_pos, self.update_notif_rect):
      self._set_state(HomeLayoutState.UPDATE)
    elif self.alert_count > 0 and rl.check_collision_point_rec(mouse_pos, self.alert_notif_rect):
      self._set_state(HomeLayoutState.ALERTS)

  def _draw_badge(self, rect, text, color, active_color, is_active):
    rl.draw_rectangle_rounded(rect, 0.3, 10, active_color if is_active else color)
    font = gui_app.font(FontWeight.MEDIUM)
    sz = measure_text_cached(font, text, HEAD_BUTTON_FONT_SIZE)
    rl.draw_text_ex(font, text, rl.Vector2(rect.x + (rect.width - sz.x)/2, rect.y + (rect.height - sz.y)/2),
                    HEAD_BUTTON_FONT_SIZE, 0, rl.WHITE)

  def _render_header(self):
    version_text_width = self.header_rect.width

    # Update notification button
    if self.update_available:
      version_text_width -= self.update_notif_rect.width
      self._draw_badge(self.update_notif_rect, tr("UPDATE"), rl.Color(54, 77, 239, 255),
                       rl.Color(75, 95, 255, 255), self.current_state == HomeLayoutState.UPDATE)

    # Alert notification button
    if self.alert_count > 0:
      version_text_width -= self.alert_notif_rect.width
      txt = trn("{} ALERT", "{} ALERTS", self.alert_count).format(self.alert_count)
      self._draw_badge(self.alert_notif_rect, txt, rl.Color(226, 44, 44, 255),
                       rl.Color(255, 70, 70, 255), self.current_state == HomeLayoutState.ALERTS)

    # Version text (right aligned)
    if self.update_available or self.alert_count > 0:
      version_text_width -= SPACING * 1.5

    version_rect = rl.Rectangle(self.header_rect.x + self.header_rect.width - version_text_width, self.header_rect.y,
                                version_text_width, self.header_rect.height)
    gui_label(version_rect, self._version_text, 48, rl.WHITE, align=Align.RIGHT, elide_right=True)

  def _render_left_column(self):
    self._prime_widget.render(self.left_col_rect)

  def _render_right_column(self):
    exp_height = 125
    exp_rect = rl.Rectangle(self.right_col_rect.x, self.right_col_rect.y, self.right_col_rect.width, exp_height)
    self._exp_mode_button.render(exp_rect)

    setup_rect = rl.Rectangle(
      self.right_col_rect.x,
      self.right_col_rect.y + exp_height + SPACING,
      self.right_col_rect.width,
      self.right_col_rect.height - exp_height - SPACING,
    )
    self._setup_widget.render(setup_rect)

  def _refresh(self):
    self.last_refresh = time.monotonic()

    self._version_text = self._get_version_text()
    update_available = self.update_alert.refresh()
    alert_count = self.offroad_alert.refresh()
    alerts_present = alert_count > 0

    # Show panels on transition from no alert/update to any alerts/update
    if not update_available and not alerts_present:
      self._set_state(HomeLayoutState.HOME)
    elif update_available and ((not self._prev_update_available) or (not alerts_present and self.current_state == HomeLayoutState.ALERTS)):
      self._set_state(HomeLayoutState.UPDATE)
    elif alerts_present and ((not self._prev_alerts_present) or (not update_available and self.current_state == HomeLayoutState.UPDATE)):
      self._set_state(HomeLayoutState.ALERTS)

    self.update_available = update_available
    self.alert_count = alert_count
    self._prev_update_available = update_available
    self._prev_alerts_present = alerts_present

  def _get_version_text(self) -> str:
    brand = "openpilot"
    description = self.params.get("UpdaterCurrentDescription")
    return f"{brand} {description}" if description else brand
