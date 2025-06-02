import pyray as rl
from dataclasses import dataclass
from enum import IntEnum
from typing import Optional, Dict, Callable, List, Tuple
from cereal import messaging
from openpilot.common.params import Params
from openpilot.system.ui.lib.application import gui_app, FontWeight
from openpilot.system.ui.layouts.settings.device import DeviceSettings
from openpilot.system.ui.layouts.settings.toggles import ToggleSettings
from openpilot.system.ui.layouts.settings.software import SoftwareSettings
from openpilot.system.ui.layouts.settings.developer import DeveloperSettings

# Import individual panels

# Constants
SIDEBAR_WIDTH = 500
CLOSE_BTN_SIZE = 200
NAV_BTN_HEIGHT = 80
PANEL_MARGIN = 50
SCROLL_SPEED = 30

# Colors
BACKGROUND_COLOR = rl.Color(0, 0, 0, 255)
SIDEBAR_COLOR = rl.Color(41, 41, 41, 255)
PANEL_COLOR = rl.Color(41, 41, 41, 255)
CLOSE_BTN_COLOR = rl.Color(41, 41, 41, 255)
CLOSE_BTN_PRESSED = rl.Color(59, 59, 59, 255)
TEXT_NORMAL = rl.Color(128, 128, 128, 255)
TEXT_SELECTED = rl.Color(255, 255, 255, 255)
TEXT_PRESSED = rl.Color(173, 173, 173, 255)


class PanelType(IntEnum):
  DEVICE = 0
  NETWORK = 1
  TOGGLES = 2
  SOFTWARE = 3
  FIREHOSE = 4
  DEVELOPER = 5


@dataclass
class PanelInfo:
  name: str
  panel_type: PanelType
  instance: object
  button_rect: rl.Rectangle = None
  enabled: bool = True


class Settings:
  def __init__(self):
    self._params = Params()
    self._current_panel = PanelType.DEVICE
    self._close_btn_pressed = False
    self._scroll_offset = 0.0
    self._max_scroll = 0.0

    # Initialize panels
    self._panels = {
      PanelType.DEVICE: DeviceSettings(self),
      # PanelType.NETWORK: NetworkPanel(self),
      PanelType.TOGGLES: ToggleSettings(self),
      PanelType.SOFTWARE: SoftwareSettings(self),
      PanelType.DEVELOPER: DeveloperSettings(self),
    }

    # Panel configuration
    self._panel_list = [
      PanelInfo("Device", PanelType.DEVICE, self._panels[PanelType.DEVICE]),
      PanelInfo("Network", PanelType.NETWORK, self._panels[PanelType.NETWORK]),
      PanelInfo("Toggles", PanelType.TOGGLES, self._panels[PanelType.TOGGLES]),
      PanelInfo("Software", PanelType.SOFTWARE, self._panels[PanelType.SOFTWARE]),
      PanelInfo("Developer", PanelType.DEVELOPER, self._panels[PanelType.DEVELOPER]),
    ]

    # UI Resources
    self._font_normal = gui_app.font(FontWeight.NORMAL)
    self._font_medium = gui_app.font(FontWeight.MEDIUM)
    self._font_bold = gui_app.font(FontWeight.SEMI_BOLD)

    # Callbacks
    self._close_callback: Optional[Callable] = None
    self._show_driver_view_callback: Optional[Callable] = None
    self._review_training_callback: Optional[Callable] = None

    # Animation state
    self._transition_progress = 0.0
    self._transitioning = False

  def set_callbacks(
    self,
    close_callback: Optional[Callable] = None,
    show_driver_view_callback: Optional[Callable] = None,
    review_training_callback: Optional[Callable] = None,
  ):
    """Set callback functions for various actions."""
    self._close_callback = close_callback
    self._show_driver_view_callback = show_driver_view_callback
    self._review_training_callback = review_training_callback

    # Pass callbacks to panels that need them
    if hasattr(self._panels[PanelType.DEVICE], 'set_callbacks'):
      self._panels[PanelType.DEVICE].set_callbacks(show_driver_view_callback, review_training_callback)

  def render(self, rect: rl.Rectangle, sm: messaging.SubMaster):
    """Main render function."""
    # Background
    rl.draw_rectangle_rec(rect, BACKGROUND_COLOR)

    # Calculate layout
    sidebar_rect = rl.Rectangle(rect.x, rect.y, SIDEBAR_WIDTH, rect.height)
    panel_rect = rl.Rectangle(rect.x + SIDEBAR_WIDTH, rect.y, rect.width - SIDEBAR_WIDTH, rect.height)

    # Draw components
    self._draw_sidebar(sidebar_rect, sm)
    self._draw_current_panel(panel_rect, sm)


  def _draw_sidebar(self, rect: rl.Rectangle, sm: messaging.SubMaster):
    """Draw the settings sidebar."""
    # Sidebar background with rounded corners
    rl.draw_rectangle_rec(rect, SIDEBAR_COLOR)

    # Close button
    close_btn_rect = rl.Rectangle(
      rect.x + (rect.width - CLOSE_BTN_SIZE) / 2, rect.y + 45, CLOSE_BTN_SIZE, CLOSE_BTN_SIZE
    )

    close_color = CLOSE_BTN_PRESSED if self._close_btn_pressed else CLOSE_BTN_COLOR
    rl.draw_rectangle_rounded(close_btn_rect, 0.5, 20, close_color)

    # Close button text (×)
    close_text_size = rl.measure_text_ex(self._font_bold, "×", 140, 0)
    close_text_pos = rl.Vector2(
      close_btn_rect.x + (close_btn_rect.width - close_text_size.x) / 2,
      close_btn_rect.y + (close_btn_rect.height - close_text_size.y) / 2 - 20,
    )
    rl.draw_text_ex(self._font_bold, "×", close_text_pos, 140, 0, TEXT_SELECTED)

    # Store close button rect for click detection
    self._close_btn_rect = close_btn_rect

    # Navigation buttons
    nav_start_y = rect.y + 300
    button_spacing = 20

    for i, panel_info in enumerate(self._panel_list):
      if not panel_info.enabled:
        continue

      button_rect = rl.Rectangle(
        rect.x + 50,
        nav_start_y + i * (NAV_BTN_HEIGHT + button_spacing),
        rect.width - 150,  # Right-aligned with margin
        NAV_BTN_HEIGHT,
      )

      # Button styling
      is_selected = panel_info.panel_type == self._current_panel
      text_color = TEXT_SELECTED if is_selected else TEXT_NORMAL

      # Draw button text (right-aligned)
      text_size = rl.measure_text_ex(self._font_medium, panel_info.name, 65, 0)
      text_pos = rl.Vector2(
        button_rect.x + button_rect.width - text_size.x, button_rect.y + (button_rect.height - text_size.y) / 2
      )
      rl.draw_text_ex(self._font_medium, panel_info.name, text_pos, 65, 0, text_color)

      # Store button rect for click detection
      panel_info.button_rect = button_rect

  def _draw_current_panel(self, rect: rl.Rectangle, sm: messaging.SubMaster):
    """Draw the currently selected panel with rounded corners."""
    # Panel background
    rl.draw_rectangle_rounded(rect, 0.03, 30, PANEL_COLOR)

    # Panel content area (with margins)
    margin = PANEL_MARGIN if self._current_panel != PanelType.NETWORK else 0
    content_rect = rl.Rectangle(rect.x + margin, rect.y + 25, rect.width - (margin * 2), rect.height - 50)

    # Render current panel with scroll support
    current_panel = self._panels.get(self._current_panel)
    if current_panel:
      # Set up scissor for scrolling
      rl.begin_scissor_mode(int(content_rect.x), int(content_rect.y), int(content_rect.width), int(content_rect.height))

      # Apply scroll offset
      scrolled_rect = rl.Rectangle(
        content_rect.x, content_rect.y - self._scroll_offset, content_rect.width, content_rect.height
      )

      current_panel.render(scrolled_rect, sm)

      rl.end_scissor_mode()

  def handle_mouse_press(self, mouse_pos: rl.Vector2) -> bool:
    """Handle mouse press events."""
    # Check close button
    if rl.check_collision_point_rec(mouse_pos, self._close_btn_rect):
      self._close_btn_pressed = True
      return True

    # Check navigation buttons
    for panel_info in self._panel_list:
      if (
        panel_info.enabled
        and panel_info.button_rect
        and rl.check_collision_point_rec(mouse_pos, panel_info.button_rect)
      ):
        self._switch_to_panel(panel_info.panel_type)
        return True

    # Forward to current panel
    current_panel = self._panels.get(self._current_panel)
    if current_panel and hasattr(current_panel, 'handle_mouse_press'):
      return current_panel.handle_mouse_press(mouse_pos)

    return False

  def handle_mouse_release(self, mouse_pos: rl.Vector2) -> bool:
    """Handle mouse release events."""
    if self._close_btn_pressed:
      self._close_btn_pressed = False
      if rl.check_collision_point_rec(mouse_pos, self._close_btn_rect):
        if self._close_callback:
          self._close_callback()
        return True

    # Forward to current panel
    current_panel = self._panels.get(self._current_panel)
    if current_panel and hasattr(current_panel, 'handle_mouse_release'):
      return current_panel.handle_mouse_release(mouse_pos)

    return False

  def handle_scroll(self, wheel_move: float, mouse_pos: rl.Vector2) -> bool:
    """Handle scroll wheel events."""
    # Only scroll if mouse is over panel area
    panel_rect = rl.Rectangle(SIDEBAR_WIDTH, 0, 9999, 9999)  # Simplified check

    if rl.check_collision_point_rec(mouse_pos, panel_rect):
      scroll_delta = wheel_move * SCROLL_SPEED
      self._scroll_offset = max(0, min(self._max_scroll, self._scroll_offset - scroll_delta))
      return True

    return False

  def _switch_to_panel(self, panel_type: PanelType):
    """Switch to a different panel with animation."""
    if panel_type != self._current_panel:
      self._current_panel = panel_type
      self._scroll_offset = 0.0  # Reset scroll when switching panels
      self._transition_progress = 0.0
      self._transitioning = True

  def set_current_panel(self, index: int, param: str = ""):
    """Set current panel by index or parameter name."""
    if param:
      # Handle parameter-based navigation (for deep links)
      if param.endswith("Panel"):
        panel_name = param[:-5]  # Remove "Panel" suffix
        for panel_info in self._panel_list:
          if panel_info.name.lower() == panel_name.lower():
            self._switch_to_panel(panel_info.panel_type)
            return
      else:
        # Expand specific toggle description
        if self._current_panel == PanelType.TOGGLES:
          toggles_panel = self._panels[PanelType.TOGGLES]
          if hasattr(toggles_panel, 'expand_toggle_description'):
            toggles_panel.expand_toggle_description(param)
    else:
      # Direct index-based navigation
      if 0 <= index < len(self._panel_list):
        self._switch_to_panel(self._panel_list[index].panel_type)

  def get_current_panel_name(self) -> str:
    """Get the name of the current panel."""
    for panel_info in self._panel_list:
      if panel_info.panel_type == self._current_panel:
        return panel_info.name
    return "Unknown"

  def update_panel_content_height(self, height: float):
    """Update the content height for scroll calculations."""
    panel_rect_height = 800  # Approximate visible height
    self._max_scroll = max(0, height - panel_rect_height)

  def close_settings(self):
    """Close the settings window."""
    if self._close_callback:
      self._close_callback()

# # Example usage integration
# def main():
#     """Test the settings layout."""
#     gui_app.init_window("Settings Test", 1920, 1080)

#     settings = SettingsLayout()
#     sm = messaging.SubMaster(['deviceState', 'pandaStates', 'carParams'])

#     # Set up callbacks
#     def close_callback():
#         print("Settings closed")
#         gui_app.close()

#     def show_driver_view():
#         print("Show driver view")

#     def review_training():
#         print("Review training guide")

#     settings.set_callbacks(close_callback, show_driver_view, review_training)

#     try:
#         for _ in gui_app.render():
#             sm.update(0)

#             # Handle input
#             if rl.is_mouse_button_pressed(rl.MouseButton.MOUSE_BUTTON_LEFT):
#                 mouse_pos = rl.Vector2(rl.get_mouse_x(), rl.get_mouse_y())
#                 settings.handle_mouse_press(mouse_pos)

#             if rl.is_mouse_button_released(rl.MouseButton.MOUSE_BUTTON_LEFT):
#                 mouse_pos = rl.Vector2(rl.get_mouse_x(), rl.get_mouse_y())
#                 settings.handle_mouse_release(mouse_pos)

#             # Handle scroll
#             wheel_move = rl.get_mouse_wheel_move()
#             if wheel_move != 0:
#                 mouse_pos = rl.Vector2(rl.get_mouse_x(), rl.get_mouse_y())
#                 settings.handle_scroll(wheel_move, mouse_pos)

#             # Draw
#             settings_rect = rl.Rectangle(0, 0, gui_app.width, gui_app.height)
#             settings.render(settings_rect, sm)

#     finally:
#         gui_app.close()


# if __name__ == "__main__":
#     main()
