import pyray as rl
import time
from enum import IntEnum
from openpilot.selfdrive.ui.layouts.sidebar import Sidebar, SIDEBAR_WIDTH
from openpilot.selfdrive.ui.layouts.home import HomeLayout
from openpilot.selfdrive.ui.layouts.settings.settings import SettingsLayout
from openpilot.selfdrive.ui.ui_state import ui_state
from openpilot.selfdrive.ui.onroad.augmented_road_view import AugmentedRoadView
from openpilot.system.ui.lib.widget import Widget


class MainState(IntEnum):
  HOME = 0
  SETTINGS = 1
  ONROAD = 2


class MainLayout(Widget):
  def __init__(self):
    super().__init__()
    self._sidebar = Sidebar()
    self._sidebar_visible = True
    self._sidebar_target_visible = True
    self._current_mode = MainState.HOME
    self._prev_onroad = False
    self._window_rect = None

    # Animation properties
    self._animation_duration = 0.25  # seconds
    self._animation_start_time = 0.0
    self._animation_start_offset = SIDEBAR_WIDTH
    self._animation_target_offset = SIDEBAR_WIDTH
    self._current_offset = SIDEBAR_WIDTH
    self._is_animating = False
    self._velocity = 0.0

    # Initialize layouts
    self._layouts = {
      MainState.HOME: HomeLayout(),
      MainState.SETTINGS: SettingsLayout(),
      MainState.ONROAD: AugmentedRoadView()
    }

    self._sidebar_rect = rl.Rectangle(0, 0, 0, 0)
    self._content_rect = rl.Rectangle(0, 0, 0, 0)

    # Set callbacks
    self._setup_callbacks()

  def _render(self, rect):
    self._update_layout_rects(rect)
    self._handle_onroad_transition()
    self._update_animation()
    self._render_main_content()

  def _setup_callbacks(self):
    self._sidebar.set_callbacks(
      on_settings=self._on_settings_clicked,
      on_flag=self._on_flag_clicked
    )
    self._layouts[MainState.SETTINGS].set_callbacks(on_close=self._set_mode_for_state)
    self._layouts[MainState.ONROAD].set_callbacks(on_click=self._on_onroad_clicked)

  def _update_layout_rects(self, rect):
    self._window_rect = rect

    # Sidebar position with animation offset
    self._sidebar_rect = rl.Rectangle(
      rect.x - (SIDEBAR_WIDTH - self._current_offset),
      rect.y,
      SIDEBAR_WIDTH,
      rect.height
    )

    # Content area adjusts based on current animated offset
    self._content_rect = rl.Rectangle(
      rect.x + self._current_offset,
      rect.y,
      rect.width - self._current_offset,
      rect.height
    )

  def _handle_onroad_transition(self):
    if ui_state.started != self._prev_onroad:
      self._prev_onroad = ui_state.started
      self._set_mode_for_state()

  def _set_sidebar_visible(self, visible: bool):
    """Animate sidebar visibility change"""
    if self._sidebar_target_visible != visible:
      self._sidebar_target_visible = visible
      self._start_animation()

  def _start_animation(self):
    """Start sidebar slide animation"""
    if self._is_animating:
      # If already animating, update the animation parameters
      current_progress = self._get_animation_progress()
      # Reverse the animation from current position
      self._animation_start_offset = self._current_offset
    else:
      self._animation_start_offset = self._current_offset

    self._is_animating = True
    self._animation_start_time = time.time()
    self._animation_target_offset = SIDEBAR_WIDTH if self._sidebar_target_visible else 0

  def _get_animation_progress(self) -> float:
    """Get current animation progress (0.0 to 1.0)"""
    if not self._is_animating:
      return 1.0

    elapsed = time.time() - self._animation_start_time
    return min(elapsed / self._animation_duration, 1.0)

  def _ease_out_cubic(self, t: float) -> float:
    """Cubic ease-out easing function"""
    return 1 - pow(1 - t, 3)

  def _ease_in_out_quart(self, t: float) -> float:
    """Quartic ease-in-out easing function for smoother animation"""
    if t < 0.5:
      return 8 * t * t * t * t
    else:
      return 1 - pow(-2 * t + 2, 4) / 2

  def _update_animation(self):
    """Update animation state with smooth easing"""
    if not self._is_animating:
      return

    progress = self._get_animation_progress()

    # Apply easing function for smooth animation
    eased_progress = self._ease_in_out_quart(progress)

    # Interpolate offset
    offset_diff = self._animation_target_offset - self._animation_start_offset
    self._current_offset = self._animation_start_offset + (offset_diff * eased_progress)

    # Check if animation is complete
    if progress >= 1.0:
      self._current_offset = self._animation_target_offset
      self._sidebar_visible = self._sidebar_target_visible
      self._is_animating = False

  def _set_mode_for_state(self):
    """Set UI mode based on current state"""
    if ui_state.started:
      self._current_mode = MainState.ONROAD
      self._set_sidebar_visible(False)
    else:
      self._current_mode = MainState.HOME
      self._set_sidebar_visible(True)

  def _on_settings_clicked(self):
    """Handle settings button click"""
    self._current_mode = MainState.SETTINGS
    self._set_sidebar_visible(False)

  def _on_flag_clicked(self):
    """Handle flag button click"""
    pass

  def _on_onroad_clicked(self):
    """Handle onroad view click to toggle sidebar"""
    self._set_sidebar_visible(not self._sidebar_target_visible)

  def _render_main_content(self):
    """Render the main content with animated sidebar"""
    # Render sidebar with clipping if partially visible
    if self._current_offset > 0:
      # Calculate visible sidebar width
      visible_width = min(self._current_offset, SIDEBAR_WIDTH)
      sidebar_x = max(self._window_rect.x, self._sidebar_rect.x)

      # Clip sidebar rendering to prevent overflow
      rl.begin_scissor_mode(
        int(sidebar_x),
        int(self._sidebar_rect.y),
        int(visible_width),
        int(self._sidebar_rect.height)
      )
      self._sidebar.render(self._sidebar_rect)
      rl.end_scissor_mode()

    # Render main content in the remaining space
    self._layouts[self._current_mode].render(self._content_rect)

    # Optional: Add subtle shadow effect during animation
    if self._is_animating and self._current_offset > 0 and self._current_offset < SIDEBAR_WIDTH:
      shadow_alpha = int(30 * (self._current_offset / SIDEBAR_WIDTH))
      shadow_color = rl.Color(0, 0, 0, shadow_alpha)
      shadow_rect = rl.Rectangle(
        self._content_rect.x - 5,
        self._content_rect.y,
        5,
        self._content_rect.height
      )
      rl.draw_rectangle_rec(shadow_rect, shadow_color)

  def open_settings(self, panel_type=None):
    """Open settings with optional panel type"""
    self._current_mode = MainState.SETTINGS
    if panel_type is not None:
      self._layouts[MainState.SETTINGS].set_current_panel(panel_type)
    self._set_sidebar_visible(False)

  def close_settings(self):
    """Close settings and return to appropriate mode"""
    self._set_mode_for_state()

  def toggle_sidebar(self):
    """Toggle sidebar visibility (useful for external calls)"""
    self._set_sidebar_visible(not self._sidebar_target_visible)

  @property
  def is_sidebar_visible(self) -> bool:
    """Check if sidebar is currently visible (or animating to visible)"""
    return self._sidebar_target_visible

  @property
  def is_animating(self) -> bool:
    """Check if sidebar animation is currently running"""
    return self._is_animating