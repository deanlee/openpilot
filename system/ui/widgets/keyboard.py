import pyray as rl
from openpilot.system.ui.lib.application import gui_app, FontWeight
from openpilot.system.ui.lib.button import ButtonStyle, gui_button
from openpilot.system.ui.lib.layout import HLayout, VLayout, Spacing, Alignment
from openpilot.system.ui.lib.inputbox import InputBox
from openpilot.system.ui.lib.label import gui_label

KEY_FONT_SIZE = 96

# Constants for special keys
CONTENT_MARGIN = 50
BACKSPACE_KEY = "<-"
ENTER_KEY = "->"
SPACE_KEY = "  "
SHIFT_KEY = "↑"
SHIFT_DOWN_KEY = "↓"
NUMERIC_KEY = "123"
SYMBOL_KEY = "#+="
ABC_KEY = "ABC"

# Define keyboard layouts as a dictionary for easier access
keyboard_layouts = {
  "lowercase": [
    ["q", "w", "e", "r", "t", "y", "u", "i", "o", "p"],
    ["a", "s", "d", "f", "g", "h", "j", "k", "l"],
    [SHIFT_KEY, "z", "x", "c", "v", "b", "n", "m", BACKSPACE_KEY],
    [NUMERIC_KEY, "/", "-", SPACE_KEY, ".", ENTER_KEY],
  ],
  "uppercase": [
    ["Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P"],
    ["A", "S", "D", "F", "G", "H", "J", "K", "L"],
    [SHIFT_DOWN_KEY, "Z", "X", "C", "V", "B", "N", "M", BACKSPACE_KEY],
    [NUMERIC_KEY, "/", "-", SPACE_KEY, ".", ENTER_KEY],
  ],
  "numbers": [
    ["1", "2", "3", "4", "5", "6", "7", "8", "9", "0"],
    ["-", "/", ":", ";", "(", ")", "$", "&", "@", "\""],
    [SYMBOL_KEY, ".", ",", "?", "!", "`", BACKSPACE_KEY],
    [ABC_KEY, SPACE_KEY, ".", ENTER_KEY],
  ],
  "specials": [
    ["[", "]", "{", "}", "#", "%", "^", "*", "+", "="],
    ["_", "\\", "|", "~", "<", ">", "€", "£", "¥", "•"],
    [NUMERIC_KEY, ".", ",", "?", "!", "'", BACKSPACE_KEY],
    [ABC_KEY, SPACE_KEY, ".", ENTER_KEY],
  ],
}


class Keyboard:
  def __init__(self, max_text_size: int = 255, min_text_size: int = 0, password_mode: bool = False, show_password_toggle: bool = False):
    self._layout = keyboard_layouts["lowercase"]
    self._max_text_size = max_text_size
    self._min_text_size = min_text_size
    self._input_box = InputBox(max_text_size)
    self._password_mode = password_mode
    self._show_password_toggle = show_password_toggle

    self._eye_open_texture = gui_app.texture("icons/eye_open.png", 81, 54)
    self._eye_closed_texture = gui_app.texture("icons/eye_closed.png", 81, 54)
    self._key_icons = {
      BACKSPACE_KEY: gui_app.texture("icons/backspace.png", 80, 80),
      SHIFT_KEY: gui_app.texture("icons/shift.png", 80, 80),
      SHIFT_DOWN_KEY: gui_app.texture("icons/arrow-down.png", 80, 80),
      ENTER_KEY: gui_app.texture("icons/arrow-right.png", 80, 80),
    }

  @property
  def text(self):
    return self._input_box.text

  def clear(self):
    self._input_box.clear()

  def render(self, title: str, sub_title: str):
    main_layout = VLayout(rl.Rectangle(0, 0, gui_app.width, gui_app.height))
    main_layout.padding = Spacing(CONTENT_MARGIN, CONTENT_MARGIN, CONTENT_MARGIN, CONTENT_MARGIN)
    main_layout.spacing = 25

    head_layout = main_layout.add_layout(HLayout(rl.Rectangle(0, 0, gui_app.width, 120)))
    head_layout.fixed_size = (gui_app.width, 120)

    tt_layout = head_layout.add_layout(VLayout(rl.Rectangle(0, 0, gui_app.width, 120)))
    title_item = tt_layout.add_fixed_item(0, 90)
    cancel_item = head_layout.add_fixed_item(386, 120)
    sub_title_item = tt_layout.add_fixed_item(gui_app.width, 60)

    input_item = main_layout.add_fixed_item(gui_app.width, 100)
    keys_item = main_layout.add_stretch_item()

    main_layout.update_layout()

    gui_label(title_item.rect, title, 90, font_weight=FontWeight.BOLD)
    gui_label(sub_title_item.rect, sub_title, 55, font_weight=FontWeight.NORMAL)
    if gui_button(cancel_item.rect, "Cancel"):
      self.clear()
      return 0

    # Draw input box and password toggle
    self._render_input_area(input_item.rect)

    h_space, v_space = 15, 15
    rect = keys_item.rect
    row_y_start = rect.y# + 300  # Starting Y position for the first row
    key_height = (rect.height - 3 * v_space) / 4

    return_code = -1
    def draw_key(key: str, key_rect: rl.Rectangle):
      nonlocal return_code
      is_enabled = key != ENTER_KEY or len(self._input_box.text) >= self._min_text_size
      result = -1
      if key in self._key_icons:
        texture = self._key_icons[key]
        result = gui_button(key_rect, "", icon=texture, button_style=ButtonStyle.PRIMARY if key == ENTER_KEY else ButtonStyle.NORMAL, is_enabled=is_enabled)
      else:
        result = gui_button(key_rect, key, is_enabled=is_enabled)

      if result:
        if key == ENTER_KEY:
          return_code = 1
        else:
          self.handle_key_press(key)

    # Iterate over the rows of keys in the current layout
    for row, keys in enumerate(self._layout):
      layout = HLayout(rl.Rectangle(rect.x, row_y_start + row * (key_height + v_space), rect.width, key_height))
      if row == 1:
        layout.padding = Spacing(80, 0, 80, 0)
      layout.spacing = 20
      layout.fixed_size = (rect.width, key_height)
      layout.alignment = (Alignment.CENTER, Alignment.CENTER)

      for i, key in enumerate(keys):
        stretch = 1.0
        if key == SPACE_KEY:
          stretch = 3.0
        elif key == ENTER_KEY:
          stretch = 2.0
        layout.add_stretch_item(100, key_height, stretch,
          lambda rect, key=key: draw_key(key, rect))

      layout.render()
    return return_code

  def _render_input_area(self, input_rect: rl.Rectangle):
    if self._show_password_toggle:
      self._input_box.set_password_mode(self._password_mode)
      self._input_box.render(rl.Rectangle(input_rect.x, input_rect.y, input_rect.width - 100, input_rect.height))

      # render eye icon
      eye_texture = self._eye_closed_texture if self._password_mode else self._eye_open_texture

      eye_rect = rl.Rectangle(input_rect.x + input_rect.width - 90, input_rect.y, 80, input_rect.height)
      eye_x = eye_rect.x + (eye_rect.width - eye_texture.width) / 2
      eye_y = eye_rect.y + (eye_rect.height - eye_texture.height) / 2

      rl.draw_texture_v(eye_texture, rl.Vector2(eye_x, eye_y), rl.WHITE)

      # Handle click on eye icon
      if rl.is_mouse_button_pressed(rl.MouseButton.MOUSE_BUTTON_LEFT) and rl.check_collision_point_rec(
        rl.get_mouse_position(), eye_rect
      ):
        self._password_mode = not self._password_mode
    else:
      self._input_box.render(input_rect)

    rl.draw_line_ex(
      rl.Vector2(input_rect.x, input_rect.y + input_rect.height - 2),
      rl.Vector2(input_rect.x + input_rect.width, input_rect.y + input_rect.height - 2),
      3.0,  # 3 pixel thickness
      rl.Color(189, 189, 189, 255),
    )

  def handle_key_press(self, key):
    if key in (SHIFT_DOWN_KEY, ABC_KEY):
      self._layout = keyboard_layouts["lowercase"]
    elif key == SHIFT_KEY:
      self._layout = keyboard_layouts["uppercase"]
    elif key == NUMERIC_KEY:
      self._layout = keyboard_layouts["numbers"]
    elif key == SYMBOL_KEY:
      self._layout = keyboard_layouts["specials"]
    elif key == BACKSPACE_KEY:
      self._input_box.delete_char_before_cursor()
    else:
      self._input_box.add_char_at_cursor(key)


if __name__ == "__main__":
  gui_app.init_window("Keyboard")
  keyboard = Keyboard(min_text_size=8)
  for _ in gui_app.render():
    result = keyboard.render("Keyboard", "Type here")
    if result == 1:
      print(f"You typed: {keyboard.text}")
      gui_app.request_close()
    elif result == 0:
      print("Canceled")
      gui_app.request_close()
  gui_app.close()
