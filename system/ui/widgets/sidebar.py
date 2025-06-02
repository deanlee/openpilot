import pyray as rl
import time
from cereal import log
from dataclasses import dataclass
from typing import Optional, Tuple
from enum import IntEnum
from cereal import messaging
from openpilot.system.ui.lib.application import gui_app, FontWeight

# Constants
SIDEBAR_WIDTH = 300
METRIC_HEIGHT = 126
METRIC_WIDTH = 240
METRIC_MARGIN = 30
BUTTON_SIZE = 192

# Colors
SIDEBAR_BG = rl.Color(57, 57, 57, 255)
GOOD_COLOR = rl.Color(0, 200, 136, 255)
WARNING_COLOR = rl.Color(201, 142, 58, 255)
DANGER_COLOR = rl.Color(201, 34, 49, 255)
WHITE = rl.Color(255, 255, 255, 255)
WHITE_DIM = rl.Color(255, 255, 255, 85)
GRAY = rl.Color(84, 84, 84, 255)
METRIC_BORDER = rl.Color(255, 255, 255, 85)

# Network types
NETWORK_TYPES = {
  0: "WiFi",
  1: "Cell",
  2: "Ethernet",
}


class ItemStatus(IntEnum):
  GOOD = 0
  WARNING = 1
  DANGER = 2


@dataclass
class MetricData:
  label: str
  value: str
  status: ItemStatus


class Sidebar:
  def __init__(self):
    self.onroad = False
    self.flag_pressed = False
    self.settings_pressed = False

    # Network state
    self.net_type = "WiFi"
    self.net_strength = 0

    # Metrics
    self.temp_status = MetricData("TEMP", "GOOD", ItemStatus.GOOD)
    self.panda_status = MetricData("VEHICLE", "ONLINE", ItemStatus.GOOD)
    self.connect_status = MetricData("CONNECT", "OFFLINE", ItemStatus.WARNING)

    # Button areas
    self.home_btn = rl.Rectangle(50, 50, BUTTON_SIZE, BUTTON_SIZE)
    self.settings_btn = rl.Rectangle(50, SIDEBAR_WIDTH - BUTTON_SIZE - 50, BUTTON_SIZE, BUTTON_SIZE)

    self.home_img = gui_app.texture("images/button_home.png", BUTTON_SIZE, BUTTON_SIZE)
    self.flag_img = gui_app.texture("images/button_flag.png", BUTTON_SIZE, BUTTON_SIZE)
    self.settings_img = gui_app.texture("images/button_settings.png", BUTTON_SIZE, BUTTON_SIZE)
    self.font_regular = gui_app.font(FontWeight.NORMAL)
    self.font_bold = gui_app.font(FontWeight.SEMI_BOLD)

    # Last update time
    self._last_update = 0.0

  def draw(self, sm, rect: rl.Rectangle):
    self.sm = sm
    self.update_state(sm)
    """Draw the sidebar."""
    # Background
    rl.draw_rectangle_rec(rect, SIDEBAR_BG)

    # Draw components
    self._draw_buttons(rect)
    self._draw_network_indicator(rect)
    self._draw_metrics(rect)

  def update_state(self, sm: messaging.SubMaster):
    """Update sidebar state from SubMaster data."""
    current_time = time.time()

    # Throttle updates to avoid excessive processing
    if current_time - self._last_update < 0.5:
      return
    self._last_update = current_time

    if not sm.valid['deviceState']:
      return

    device_state = sm['deviceState']

    # Update network status
    self._update_network_status(device_state)

    # Update metrics
    self._update_temperature_status(device_state)
    self._update_connection_status(device_state)
    self._update_panda_status(sm)

  def _update_network_status(self, device_state):
    """Update network type and strength."""
    network_type = device_state.networkType
    self.net_type = NETWORK_TYPES.get(network_type, "Unknown")

    strength = device_state.networkStrength
    self.net_strength = max(0, min(5, strength.raw + 1)) if strength > 0 else 0

  def _update_temperature_status(self, device_state):
    """Update temperature status."""
    thermal_status = device_state.thermalStatus

    if thermal_status == log.thermalStatus.green:
      self.temp_status = MetricData("TEMP", "GOOD", ItemStatus.GOOD)
    elif thermal_status == log.thermalStatus.yellow:
      self.temp_status = MetricData("TEMP", "OK", ItemStatus.WARNING)
    else:
      self.temp_status = MetricData("TEMP", "HIGH", ItemStatus.DANGER)

  def _update_connection_status(self, device_state):
    """Update connection status."""
    last_ping = device_state.lastAthenaPingTime
    current_time_ns = time.time_ns()

    if last_ping == 0:
      self.connect_status = MetricData("CONNECT", "OFFLINE", ItemStatus.WARNING)
    elif current_time_ns - last_ping < 80_000_000_000:  # 80 seconds in nanoseconds
      self.connect_status = MetricData("CONNECT", "ONLINE", ItemStatus.GOOD)
    else:
      self.connect_status = MetricData("CONNECT", "ERROR", ItemStatus.DANGER)

  def _update_panda_status(self, sm):
    """Update panda connection status."""
    if sm.valid['pandaStates'] and len(sm['pandaStates']) > 0:
      panda_state = sm['pandaStates'][0]
      if hasattr(panda_state, 'pandaType') and panda_state.pandaType != 0:  # UNKNOWN
        self.panda_status = MetricData("VEHICLE", "ONLINE", ItemStatus.GOOD)
      else:
        self.panda_status = MetricData("NO", "PANDA", ItemStatus.DANGER)
    else:
      self.panda_status = MetricData("NO", "PANDA", ItemStatus.DANGER)

  def handle_mouse_press(self, mouse_pos: Tuple[float, float]) -> Optional[str]:
    """Handle mouse press events. Returns action if any."""
    x, y = mouse_pos

    if rl.check_collision_point_rec(rl.Vector2(x, y), self.home_btn):
      if self.onroad:
        self.flag_pressed = True
        return "flag"
      else:
        return "home"
    elif rl.check_collision_point_rec(rl.Vector2(x, y), self.settings_btn):
      self.settings_pressed = True
      return "settings"

    return None

  def handle_mouse_release(self, mouse_pos: Tuple[float, float]) -> Optional[str]:
    """Handle mouse release events. Returns action if any."""
    x, y = mouse_pos
    action = None

    if self.flag_pressed:
      self.flag_pressed = False
      if rl.check_collision_point_rec(rl.Vector2(x, y), self.home_btn):
        action = "send_flag"

    if self.settings_pressed:
      self.settings_pressed = False
      if rl.check_collision_point_rec(rl.Vector2(x, y), self.settings_btn):
        action = "open_settings"

    return action

  def set_onroad(self, onroad: bool):
    """Set onroad status."""
    self.onroad = onroad

  def _draw_buttons(self, rect: rl.Rectangle):
    """Draw navigation buttons."""
    # Settings button
    opacity = 0.65 if self.settings_pressed else 1.0

    if self.settings_img:
      tint = rl.Color(255, 255, 255, int(255 * opacity))
      rl.draw_texture_rec(
        self.settings_img,
        rl.Rectangle(0, 0, float(self.settings_img.width), float(self.settings_img.height)),
        rl.Vector2(rect.x + self.settings_btn.x, rect.y + self.settings_btn.y),
        tint,
      )
    else:
      # Fallback: draw colored rectangle
      color = rl.Color(100, 100, 100, int(255 * opacity))
      rl.draw_rectangle_rec(
        rl.Rectangle(
          rect.x + self.settings_btn.x, rect.y + self.settings_btn.y, self.settings_btn.width, self.settings_btn.height
        ),
        color,
      )

    # Home/Flag button
    opacity = 0.65 if self.onroad and self.flag_pressed else 1.0
    button_img = self.flag_img if self.onroad else self.home_img

    if button_img:
      tint = rl.Color(255, 255, 255, int(255 * opacity))
      rl.draw_texture_rec(
        button_img,
        rl.Rectangle(0, 0, float(button_img.width), float(button_img.height)),
        rl.Vector2(rect.x + self.home_btn.x, rect.y + self.home_btn.y),
        tint,
      )
    else:
      # Fallback: draw colored rectangle
      color = rl.Color(150, 100, 50, int(255 * opacity)) if self.onroad else rl.Color(100, 150, 100, int(255 * opacity))
      rl.draw_rectangle_rec(
        rl.Rectangle(rect.x + self.home_btn.x, rect.y + self.home_btn.y, self.home_btn.width, self.home_btn.height),
        color,
      )

  def _draw_network_indicator(self, rect: rl.Rectangle):
    """Draw network strength indicator and type."""
    # Signal strength dots
    x_start = rect.x + 58
    y_pos = rect.y + 196
    dot_size = 27
    dot_spacing = 37

    for i in range(5):
      color = WHITE if i < self.net_strength else GRAY
      rl.draw_circle(int(x_start + i * dot_spacing + dot_size // 2), int(y_pos + dot_size // 2), dot_size // 2, color)

    # Network type text
    text_y = rect.y + 247
    text_rect = rl.Rectangle(rect.x + 58, text_y, rect.width - 100, 50)
    self._draw_text_in_rect(self.net_type, self.font_regular, 35, text_rect, WHITE, rl.GuiTextAlignment.TEXT_ALIGN_LEFT)

  def _draw_metrics(self, rect: rl.Rectangle):
    """Draw status metrics."""
    metrics = [(self.temp_status, 338), (self.panda_status, 496), (self.connect_status, 654)]

    for metric, y_offset in metrics:
      self._draw_metric(rect, metric, rect.y + y_offset)

  def _draw_metric(self, rect: rl.Rectangle, metric: MetricData, y: float):
    """Draw a single metric box."""
    metric_rect = rl.Rectangle(rect.x + METRIC_MARGIN, y, METRIC_WIDTH, METRIC_HEIGHT)

    # Get status color
    status_colors = {
      ItemStatus.GOOD: GOOD_COLOR,
      ItemStatus.WARNING: WARNING_COLOR,
      ItemStatus.DANGER: DANGER_COLOR,
    }
    status_color = status_colors.get(metric.status, GRAY)

    # Draw colored left edge (clipped rounded rectangle)
    edge_rect = rl.Rectangle(metric_rect.x + 4, metric_rect.y + 4, 100, 118)
    rl.begin_scissor_mode(int(metric_rect.x + 4), int(metric_rect.y), 18, int(metric_rect.height))
    rl.draw_rectangle_rounded(edge_rect, 0.18, 10, status_color)
    rl.end_scissor_mode()

    # Draw border
    rl.draw_rectangle_rounded_lines_ex(metric_rect, 0.15, 10, 2, METRIC_BORDER)

    # Draw text
    text = f"{metric.label}\n{metric.value}"
    text_rect = rl.Rectangle(metric_rect.x + 22, metric_rect.y, metric_rect.width - 22, metric_rect.height)
    self._draw_text_in_rect(text, self.font_bold, 35, text_rect, WHITE, rl.GuiTextAlignment.TEXT_ALIGN_CENTER)

  def _draw_text_in_rect(
    self, text: str, font: rl.Font, size: int, rect: rl.Rectangle, color: rl.Color, alignment: int
  ):
    """Draw text within a rectangle with specified alignment."""
    text_size = rl.measure_text_ex(font, text, size, 0)

    # Calculate position based on alignment
    if alignment == rl.GuiTextAlignment.TEXT_ALIGN_CENTER:
      x = rect.x + (rect.width - text_size.x) / 2
    elif alignment == rl.GuiTextAlignment.TEXT_ALIGN_LEFT:
      x = rect.x
    else:  # RIGHT
      x = rect.x + rect.width - text_size.x

    y = rect.y + (rect.height - text_size.y) / 2

    rl.draw_text_ex(font, text, rl.Vector2(x, y), size, 0, color)


if __name__ == "__main__":
  gui_app.init_window("OnRoad Camera View")
  sm = messaging.SubMaster(["modelV2", "controlsState", "liveCalibration", "radarState", "deviceState",
    "pandaStates", "carParams", "driverMonitoringState", "carState", "driverStateV2",
    "roadCameraState", "wideRoadCameraState", "managerState", "selfdriveState", "longitudinalPlan"])
  sidbar = Sidebar()
  for _ in gui_app.render():
    sm.update(0)
    sidbar.draw(sm, rl.Rectangle(0, 0, 500, gui_app.height))
