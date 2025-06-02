import pyray as rl
from dataclasses import dataclass
from typing import Optional, Callable, List
from cereal import messaging, log
from openpilot.common.params import Params
from openpilot.system.ui.lib.application import gui_app, FontWeight
from openpilot.selfdrive.car.car_helpers import get_car_interface

# Constants
TOGGLE_HEIGHT = 120
TOGGLE_SPACING = 20
CONTENT_MARGIN = 40
SCROLL_SPEED = 30

# Colors
PANEL_BG = rl.Color(41, 41, 41, 255)
CARD_BG = rl.Color(57, 57, 57, 255)
TOGGLE_ENABLED = rl.Color(23, 134, 68, 255)
TOGGLE_DISABLED = rl.Color(100, 100, 100, 255)
TEXT_PRIMARY = rl.Color(255, 255, 255, 255)
TEXT_SECONDARY = rl.Color(180, 180, 180, 255)
TEXT_WARNING = rl.Color(255, 180, 0, 255)
BORDER_COLOR = rl.Color(70, 70, 70, 255)


@dataclass
class ToggleItem:
  param_name: str
  title: str
  description: str
  warning: str = ""
  enabled: bool = True
  visible: bool = True
  confirmation_required: bool = False
  on_toggle: Optional[Callable[[bool], None]] = None
  rect: rl.Rectangle = None


class DeveloperPanel:
  def __init__(self, settings_window=None):
    self._settings_window = settings_window
    self._params = Params()
    self._offroad = True
    self._is_release = self._params.get_bool("IsReleaseBranch")
    self._scroll_offset = 0.0
    self._content_height = 0.0

    # UI resources
    self._font_regular = gui_app.font(FontWeight.NORMAL)
    self._font_medium = gui_app.font(FontWeight.MEDIUM)
    self._font_bold = gui_app.font(FontWeight.SEMI_BOLD)

    # Confirmation dialog state
    self._confirmation_dialog = None
    self._pending_toggle = None

    # Initialize toggles
    self._init_toggles()

    # Update initial state
    self._update_toggles(self._offroad)

  def _init_toggles(self):
    """Initialize all developer toggles."""
    self._toggles: List[ToggleItem] = [
      ToggleItem(
        param_name="AdbEnabled",
        title="Enable ADB",
        description="ADB (Android Debug Bridge) allows connecting to your device over USB or over the network. See https://docs.comma.ai/how-to/connect-to-comma for more info.",
        enabled=not self._is_release,
      ),
      # SSH Keys section would go here - simplified for now
      ToggleItem(
        param_name="SshEnabled",
        title="SSH Keys",
        description="Manage SSH keys for secure remote access",
        enabled=not self._is_release,
      ),
      ToggleItem(
        param_name="JoystickDebugMode",
        title="Joystick Debug Mode",
        description="Enable joystick control for debugging purposes",
        enabled=not self._is_release,
        on_toggle=self._on_joystick_toggle,
      ),
      ToggleItem(
        param_name="LongitudinalManeuverMode",
        title="Longitudinal Maneuver Mode",
        description="Enable manual longitudinal control for testing",
        enabled=not self._is_release,
        on_toggle=self._on_long_maneuver_toggle,
      ),
      ToggleItem(
        param_name="AlphaLongitudinalEnabled",
        title="openpilot Longitudinal Control (Alpha)",
        description="On this car, openpilot defaults to the car's built-in ACC instead of openpilot's longitudinal control. Enable this to switch to openpilot longitudinal control. Enabling Experimental mode is recommended when enabling openpilot longitudinal control alpha.",
        warning="WARNING: openpilot longitudinal control is in alpha for this car and will disable Automatic Emergency Braking (AEB).",
        confirmation_required=True,
        on_toggle=self._on_experimental_long_toggle,
      ),
    ]

  def render(self, rect: rl.Rectangle, sm: messaging.SubMaster):
    """Render the developer panel."""
    # Update state
    self._update_offroad_state(sm)

    # Background
    rl.draw_rectangle_rec(rect, PANEL_BG)

    # Calculate content area
    content_rect = rl.Rectangle(
      rect.x + CONTENT_MARGIN,
      rect.y + CONTENT_MARGIN,
      rect.width - CONTENT_MARGIN * 2,
      rect.height - CONTENT_MARGIN * 2,
    )

    # Enable scissor for scrolling
    rl.begin_scissor_mode(int(content_rect.x), int(content_rect.y), int(content_rect.width), int(content_rect.height))

    # Render toggles with scroll offset
    current_y = content_rect.y - self._scroll_offset

    for toggle in self._toggles:
      if not toggle.visible:
        continue

      toggle_rect = rl.Rectangle(content_rect.x, current_y, content_rect.width, TOGGLE_HEIGHT)

      # Store rect for click detection
      toggle.rect = rl.Rectangle(
        toggle_rect.x,
        toggle_rect.y + self._scroll_offset,  # Adjust for scroll offset
        toggle_rect.width,
        toggle_rect.height,
      )

      self._draw_toggle(toggle_rect, toggle)
      current_y += TOGGLE_HEIGHT + TOGGLE_SPACING

    # Update content height
    self._content_height = current_y - content_rect.y + self._scroll_offset

    rl.end_scissor_mode()

    # Draw scroll indicators
    self._draw_scroll_indicators(content_rect)

    # Draw confirmation dialog if active
    if self._confirmation_dialog:
      self._draw_confirmation_dialog(rect)

  def _draw_toggle(self, rect: rl.Rectangle, toggle: ToggleItem):
    """Draw a single toggle item."""
    # Card background
    card_color = CARD_BG if toggle.enabled else rl.Color(40, 40, 40, 255)
    rl.draw_rectangle_rounded(rect, 0.05, 10, card_color)
    rl.draw_rectangle_rounded_lines(rect, 0.05, 10, 2, BORDER_COLOR)

    # Get current value
    current_value = self._params.get_bool(toggle.param_name)

    # Toggle switch area
    switch_width, switch_height = 80, 40
    switch_rect = rl.Rectangle(rect.x + rect.width - switch_width - 20, rect.y + 20, switch_width, switch_height)

    # Switch background
    switch_bg = TOGGLE_ENABLED if (current_value and toggle.enabled) else TOGGLE_DISABLED
    rl.draw_rectangle_rounded(switch_rect, 0.5, 15, switch_bg)

    # Switch knob
    knob_size = switch_height - 8
    knob_x = (switch_rect.x + switch_width - knob_size - 4) if current_value else (switch_rect.x + 4)
    knob_rect = rl.Rectangle(knob_x, switch_rect.y + 4, knob_size, knob_size)
    knob_color = rl.WHITE if toggle.enabled else rl.Color(200, 200, 200, 255)
    rl.draw_rectangle_rounded(knob_rect, 0.5, 10, knob_color)

    # Text area
    text_area = rl.Rectangle(rect.x + 20, rect.y + 15, rect.width - switch_width - 60, rect.height - 30)

    # Title
    title_color = TEXT_PRIMARY if toggle.enabled else TEXT_SECONDARY
    title_size = rl.measure_text_ex(self._font_medium, toggle.title, 28, 0)
    rl.draw_text_ex(self._font_medium, toggle.title, rl.Vector2(text_area.x, text_area.y), 28, 0, title_color)

    # Warning text (if applicable)
    current_text_y = text_area.y + title_size.y + 8
    if toggle.warning and current_value:
      warning_lines = self._wrap_text(toggle.warning, text_area.width, 18)
      for line in warning_lines:
        rl.draw_text_ex(self._font_regular, line, rl.Vector2(text_area.x, current_text_y), 18, 0, TEXT_WARNING)
        current_text_y += 22
      current_text_y += 5

    # Description
    if toggle.description:
      desc_lines = self._wrap_text(toggle.description, text_area.width, 18)
      desc_color = TEXT_SECONDARY if toggle.enabled else rl.Color(120, 120, 120, 255)

      for line in desc_lines:
        rl.draw_text_ex(self._font_regular, line, rl.Vector2(text_area.x, current_text_y), 18, 0, desc_color)
        current_text_y += 22

  def _draw_confirmation_dialog(self, rect: rl.Rectangle):
    """Draw confirmation dialog overlay."""
    # Overlay background
    rl.draw_rectangle_rec(rect, rl.Color(0, 0, 0, 150))

    # Dialog box
    dialog_width, dialog_height = 600, 400
    dialog_rect = rl.Rectangle(
      rect.x + (rect.width - dialog_width) / 2, rect.y + (rect.height - dialog_height) / 2, dialog_width, dialog_height
    )

    rl.draw_rectangle_rounded(dialog_rect, 0.05, 15, CARD_BG)
    rl.draw_rectangle_rounded_lines(dialog_rect, 0.05, 15, 3, BORDER_COLOR)

    # Dialog content
    toggle = self._pending_toggle
    if toggle:
      # Title
      title_y = dialog_rect.y + 30
      self._draw_centered_text(
        "Confirm Action", self._font_bold, 32, rl.Rectangle(dialog_rect.x, title_y, dialog_rect.width, 40), TEXT_PRIMARY
      )

      # Warning/Description
      content_y = title_y + 60
      content_area = rl.Rectangle(dialog_rect.x + 30, content_y, dialog_rect.width - 60, 200)

      if toggle.warning:
        warning_lines = self._wrap_text(toggle.warning, content_area.width, 20)
        for line in warning_lines:
          rl.draw_text_ex(self._font_regular, line, rl.Vector2(content_area.x, content_y), 20, 0, TEXT_WARNING)
          content_y += 25
        content_y += 15

      if toggle.description:
        desc_lines = self._wrap_text(toggle.description, content_area.width, 18)
        for line in desc_lines:
          rl.draw_text_ex(self._font_regular, line, rl.Vector2(content_area.x, content_y), 18, 0, TEXT_SECONDARY)
          content_y += 22

      # Buttons
      button_y = dialog_rect.y + dialog_rect.height - 80
      button_width, button_height = 120, 50

      # Cancel button
      cancel_rect = rl.Rectangle(
        dialog_rect.x + dialog_rect.width / 2 - button_width - 10, button_y, button_width, button_height
      )
      rl.draw_rectangle_rounded(cancel_rect, 0.1, 10, TOGGLE_DISABLED)
      self._draw_centered_text("Cancel", self._font_medium, 22, cancel_rect, TEXT_PRIMARY)

      # Confirm button
      confirm_rect = rl.Rectangle(dialog_rect.x + dialog_rect.width / 2 + 10, button_y, button_width, button_height)
      rl.draw_rectangle_rounded(confirm_rect, 0.1, 10, TOGGLE_ENABLED)
      self._draw_centered_text("Confirm", self._font_medium, 22, confirm_rect, TEXT_PRIMARY)

      # Store rects for click detection
      self._cancel_btn_rect = cancel_rect
      self._confirm_btn_rect = confirm_rect

  def _draw_scroll_indicators(self, rect: rl.Rectangle):
    """Draw scroll indicators if content overflows."""
    max_scroll = max(0, self._content_height - rect.height)
    if max_scroll <= 0:
      return

    # Scroll bar
    bar_width = 6
    bar_height = max(30, rect.height * (rect.height / self._content_height))
    bar_y = rect.y + (self._scroll_offset / max_scroll) * (rect.height - bar_height)

    bar_rect = rl.Rectangle(rect.x + rect.width - bar_width - 5, bar_y, bar_width, bar_height)

    rl.draw_rectangle_rounded(bar_rect, 0.5, 3, rl.Color(255, 255, 255, 100))

  def _wrap_text(self, text: str, max_width: float, font_size: int) -> List[str]:
    """Wrap text to fit within max_width."""
    words = text.split(' ')
    lines = []
    current_line = ""

    for word in words:
      test_line = f"{current_line} {word}".strip()
      test_width = rl.measure_text_ex(self._font_regular, test_line, font_size, 0).x

      if test_width <= max_width:
        current_line = test_line
      else:
        if current_line:
          lines.append(current_line)
        current_line = word

    if current_line:
      lines.append(current_line)

    return lines

  def _draw_centered_text(self, text: str, font: rl.Font, size: int, rect: rl.Rectangle, color: rl.Color):
    """Draw text centered in rectangle."""
    text_size = rl.measure_text_ex(font, text, size, 0)
    pos = rl.Vector2(rect.x + (rect.width - text_size.x) / 2, rect.y + (rect.height - text_size.y) / 2)
    rl.draw_text_ex(font, text, pos, size, 0, color)

  def handle_mouse_press(self, mouse_pos: rl.Vector2) -> bool:
    """Handle mouse press events."""
    # Handle confirmation dialog first
    if self._confirmation_dialog:
      return self._handle_dialog_click(mouse_pos)

    # Check toggle switches
    for toggle in self._toggles:
      if toggle.visible and toggle.enabled and toggle.rect and rl.check_collision_point_rec(mouse_pos, toggle.rect):
        self._toggle_parameter(toggle)
        return True

    return False

  def handle_scroll(self, wheel_move: float) -> bool:
    """Handle scroll wheel events."""
    if self._confirmation_dialog:
      return False

    max_scroll = max(0, self._content_height - 600)  # Approximate visible height
    self._scroll_offset = max(0, min(max_scroll, self._scroll_offset - wheel_move * SCROLL_SPEED))
    return True

  def _handle_dialog_click(self, mouse_pos: rl.Vector2) -> bool:
    """Handle clicks in confirmation dialog."""
    if hasattr(self, '_cancel_btn_rect') and rl.check_collision_point_rec(mouse_pos, self._cancel_btn_rect):
      self._confirmation_dialog = None
      self._pending_toggle = None
      return True

    if hasattr(self, '_confirm_btn_rect') and rl.check_collision_point_rec(mouse_pos, self._confirm_btn_rect):
      if self._pending_toggle:
        # Actually toggle the parameter
        current_value = self._params.get_bool(self._pending_toggle.param_name)
        self._params.put_bool(self._pending_toggle.param_name, not current_value)

        # Call toggle callback
        if self._pending_toggle.on_toggle:
          self._pending_toggle.on_toggle(not current_value)

      self._confirmation_dialog = None
      self._pending_toggle = None
      return True

    return False

  def _toggle_parameter(self, toggle: ToggleItem):
    """Toggle a parameter, showing confirmation if required."""
    if toggle.confirmation_required:
      self._confirmation_dialog = True
      self._pending_toggle = toggle
    else:
      current_value = self._params.get_bool(toggle.param_name)
      self._params.put_bool(toggle.param_name, not current_value)

      if toggle.on_toggle:
        toggle.on_toggle(not current_value)

  def _on_joystick_toggle(self, enabled: bool):
    """Handle joystick toggle - disable longitudinal maneuver mode."""
    if enabled:
      self._params.put_bool("LongitudinalManeuverMode", False)

  def _on_long_maneuver_toggle(self, enabled: bool):
    """Handle longitudinal maneuver toggle - disable joystick mode."""
    if enabled:
      self._params.put_bool("JoystickDebugMode", False)

  def _on_experimental_long_toggle(self, enabled: bool):
    """Handle experimental longitudinal toggle."""
    # Update toggles after change
    self._update_toggles(self._offroad)

  def _update_offroad_state(self, sm: messaging.SubMaster):
    """Update offroad state from SubMaster."""
    if sm.valid['deviceState']:
      # In real implementation, this would check actual offroad state
      # For now, assume offroad
      self._offroad = True

  def _update_toggles(self, offroad: bool):
    """Update toggle visibility and enabled state based on car capabilities and offroad state."""
    self._offroad = offroad

    # Update basic toggle states
    for toggle in self._toggles:
      # Most toggles are only enabled during offroad
      if toggle.param_name != "AlphaLongitudinalEnabled":
        toggle.enabled = offroad and not self._is_release

      # All toggles hidden on release branch except experimental long
      if self._is_release and toggle.param_name != "AlphaLongitudinalEnabled":
        toggle.visible = False

    # Check car capabilities
    car_params_bytes = self._params.get("CarParamsPersistent")
    if car_params_bytes:
      try:
        # Parse car params
        msg = messaging.log_from_bytes(car_params_bytes, log.CarParams)

        # Handle experimental longitudinal toggle
        exp_long_toggle = next((t for t in self._toggles if t.param_name == "AlphaLongitudinalEnabled"), None)
        if exp_long_toggle:
          alpha_available = msg.alphaLongitudinalAvailable

          if not alpha_available or self._is_release:
            self._params.remove("AlphaLongitudinalEnabled")
            exp_long_toggle.enabled = False

          exp_long_toggle.visible = alpha_available and not self._is_release

        # Handle longitudinal maneuver toggle
        long_maneuver_toggle = next((t for t in self._toggles if t.param_name == "LongitudinalManeuverMode"), None)
        if long_maneuver_toggle:
          has_long_control = self._has_longitudinal_control(msg)
          long_maneuver_toggle.enabled = has_long_control and offroad and not self._is_release

      except Exception as e:
        print(f"Error parsing CarParams: {e}")
        # Disable relevant toggles on error
        for param_name in ["AlphaLongitudinalEnabled", "LongitudinalManeuverMode"]:
          toggle = next((t for t in self._toggles if t.param_name == param_name), None)
          if toggle:
            toggle.enabled = False
            if param_name == "AlphaLongitudinalEnabled":
              toggle.visible = False
    else:
      # No car params available - disable car-specific toggles
      for param_name in ["AlphaLongitudinalEnabled", "LongitudinalManeuverMode"]:
        toggle = next((t for t in self._toggles if t.param_name == param_name), None)
        if toggle:
          toggle.enabled = False
          if param_name == "AlphaLongitudinalEnabled":
            toggle.visible = False

  def _has_longitudinal_control(self, car_params) -> bool:
    """Check if car has longitudinal control capability."""
    # This would implement the hasLongitudinalControl logic from the C++ version
    return hasattr(car_params, 'longitudinalTuning') and car_params.longitudinalTuning is not None

  def show_event(self):
    """Called when panel is shown - equivalent to Qt's showEvent."""
    self._update_toggles(self._offroad)



# class DeveloperSettings:
#   def __init__(self):
#     pass
