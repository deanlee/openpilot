MAX_VELOCITY = 80.0
RUBBER_BAND_FACTOR = 0.4  # Lower is stretchier
SNAP_BACK_SPEED = 0.25    # Higher is snappier

class GuiScrollPanel:
  def __init__(self, show_vertical_scroll_bar: bool = False):
    self._scroll_state: ScrollState = ScrollState.IDLE
    self._last_mouse_y: float = 0.0
    self._start_mouse_y: float = 0.0  # Track the initial mouse position for drag detection
    self._offset = rl.Vector2(0, 0)
    self._view = rl.Rectangle(0, 0, 0, 0)
    self._show_vertical_scroll_bar: bool = show_vertical_scroll_bar
    self._velocity_y = 0.0  # Velocity for inertia
    self._is_dragging = False

  def handle_scroll(self, bounds: rl.Rectangle, content: rl.Rectangle) -> rl.Vector2:
    mouse_pos = rl.get_mouse_position()

    # Handle dragging logic
    if rl.check_collision_point_rec(mouse_pos, bounds) and rl.is_mouse_button_pressed(rl.MouseButton.MOUSE_BUTTON_LEFT):
      if self._scroll_state == ScrollState.IDLE:
        self._scroll_state = ScrollState.DRAGGING_CONTENT
        if self._show_vertical_scroll_bar:
          scrollbar_width = rl.gui_get_style(rl.GuiControl.LISTVIEW, rl.GuiListViewProperty.SCROLLBAR_WIDTH)
          scrollbar_x = bounds.x + bounds.width - scrollbar_width
          if mouse_pos.x >= scrollbar_x:
            self._scroll_state = ScrollState.DRAGGING_SCROLLBAR

        self._last_mouse_y = mouse_pos.y
        self._start_mouse_y = mouse_pos.y  # Record starting position
        self._velocity_y = 0.0  # Reset velocity when drag starts
        self._is_dragging = False  # Reset dragging flag

    if self._scroll_state != ScrollState.IDLE:
      if rl.is_mouse_button_down(rl.MouseButton.MOUSE_BUTTON_LEFT):
        delta_y = mouse_pos.y - self._last_mouse_y

        # Check if movement exceeds the drag threshold
        total_drag = abs(mouse_pos.y - self._start_mouse_y)
        if total_drag > DRAG_THRESHOLD:
          self._is_dragging = True

        if self._scroll_state == ScrollState.DRAGGING_CONTENT:
          # Rubber-banding when out of bounds
          max_scroll_y = max(content.height - bounds.height, 0)
          over_scroll = 0
          if self._offset.y > 0:
            over_scroll = self._offset.y
          elif self._offset.y < -max_scroll_y:
            over_scroll = self._offset.y + max_scroll_y
          if over_scroll != 0:
            delta_y *= RUBBER_BAND_FACTOR

          self._offset.y += delta_y
        elif self._scroll_state == ScrollState.DRAGGING_SCROLLBAR:
          delta_y = -delta_y

        # Use difference between last two positions for velocity
        self._velocity_y = max(min(delta_y, MAX_VELOCITY), -MAX_VELOCITY)
        self._last_mouse_y = mouse_pos.y
      elif rl.is_mouse_button_released(rl.MouseButton.MOUSE_BUTTON_LEFT):
        self._scroll_state = ScrollState.IDLE

    # Handle mouse wheel scrolling
    wheel_move = rl.get_mouse_wheel_move()
    if self._show_vertical_scroll_bar:
      self._offset.y += wheel_move * (MOUSE_WHEEL_SCROLL_SPEED - 20)
      rl.gui_scroll_panel(bounds, rl.ffi.NULL, content, self._offset, self._view)
    else:
      self._offset.y += wheel_move * MOUSE_WHEEL_SCROLL_SPEED

    max_scroll_y = max(content.height - bounds.height, 0)

    # Inertia and snap-back
    if self._scroll_state == ScrollState.IDLE:
      self._offset.y += self._velocity_y
      self._velocity_y *= INERTIA_FRICTION

      # Rubber-band snap-back if out of bounds
      if self._offset.y > 0:
        self._offset.y -= self._offset.y * SNAP_BACK_SPEED
        if abs(self._offset.y) < 1.0:
          self._offset.y = 0
          self._velocity_y = 0
      elif self._offset.y < -max_scroll_y:
        over = self._offset.y + max_scroll_y
        self._offset.y -= over * SNAP_BACK_SPEED
        if abs(over) < 1.0:
          self._offset.y = -max_scroll_y
          self._velocity_y = 0

      # Stop scrolling when velocity is low and in bounds
      if abs(self._velocity_y) < MIN_VELOCITY and 0 >= self._offset.y >= -max_scroll_y:
        self._velocity_y = 0.0

    return self._offset