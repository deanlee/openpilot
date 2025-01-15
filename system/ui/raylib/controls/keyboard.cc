#include "system/ui/raylib/controls/keyboard.h"

#include "raylib.h"
#include "system/ui/raylib/util.h"
#define RAYGUI_IMPLEMENTATION
#define BLANK RAYLIB_BLANK
#define RAYGUI_WINDOWBOX_STATUSBAR_HEIGHT 50
#define RAYGUI_MESSAGEBOX_BUTTON_HEIGHT 100
#define RAYGUI_MESSAGEBOX_BUTTON_PADDING 30
#define RAYGUI_TEXTINPUTBOX_BUTTON_PADDING 50
#define RAYGUI_TEXTINPUTBOX_BUTTON_HEIGHT 100
#define RAYGUI_TEXTINPUTBOX_HEIGHT 40

#include "third_party/raylib/include/raygui.h"

const std::string BACKSPACE_KEY = "⌫";
const std::string ENTER_KEY = "→";

std::vector<std::vector<std::string>> lowercase = {
    {"q", "w", "e", "r", "t", "y", "u", "i", "o", "p"},
    {"a", "s", "d", "f", "g", "h", "j", "k", "l"},
    {"↑", "z", "x", "c", "v", "b", "n", "m", BACKSPACE_KEY},
    {"123", "/", "-", "  ", ".", ENTER_KEY},
};

std::vector<std::vector<std::string>> uppercase = {
    {"Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P"},
    {"A", "S", "D", "F", "G", "H", "J", "K", "L"},
    {"↓", "Z", "X", "C", "V", "B", "N", "M", BACKSPACE_KEY},
    {"123", "/", "-", "  ", ".", ENTER_KEY},
};

std::vector<std::vector<std::string>> numbers = {
    {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0"},
    {"-", "/", ":", ";", "(", ")", "$", "&&", "@", "\""},
    {"#+=", ".", ",", "?", "!", "`", BACKSPACE_KEY},
    {"ABC", "  ", ".", ENTER_KEY},
};

std::vector<std::vector<std::string>> specials = {
    {"[", "]", "{", "}", "#", "%", "^", "*", "+", "="},
    {"_", "\\", "|", "~", "<", ">", "€", "£", "¥", "•"},
    {"123", ".", ",", "?", "!", "'", BACKSPACE_KEY},
    {"ABC", "  ", ".", ENTER_KEY},
};

Keyboard::Keyboard() {
  layout = &lowercase;
  GuiSetFont(pApp->getFont());
  GuiSetStyle(DEFAULT, TEXT_SIZE, 40);
}

void Keyboard::render(const Rectangle &rect, const std::string &title, const std::string &sub_title) {
  GuiTextBox((Rectangle){rect.x, rect.y, rect.width, 30}, inputText, 256, true);

  float keyHeight = 155;
  // float h_space = 10;
  float v_space = 10;
  float key_max_width = (float)rect.width / layout->at(2).size();

  for (int row = 0; row < layout->size(); row++) {
    const auto &keys = layout->at(row);
    int start_x = row == 1 ? rect.x + 90 : rect.x;
    float key_width = row == 1 ? (rect.width - 180) / keys.size() : (rect.width) / keys.size();
    key_width = std::min(key_width, key_max_width);
    for (int col = 0; col < keys.size(); col++) {
      int new_width = key_width;
      if (keys[col][0] == ' ') {
        new_width = key_width * 3;
      } else if (keys[col] == ENTER_KEY) {
        new_width = key_width * 2;
      }
      Rectangle keyRect = {(float)start_x, rect.y + 40 + row * (keyHeight + v_space), (float)new_width, keyHeight};
      start_x += new_width;
      if (GuiButton(keyRect, keys[col].c_str())) {
        auto key = keys[col];
        if (key == "↓" || key == "ABC") {
          layout = &lowercase;
        } else if (key == "↑") {
          layout = &uppercase;
        } else if (key == "123") {
          layout = &numbers;
        } else if (key == "#+=") {
          layout = &specials;
        } else {
          int len = strlen(inputText);
          if (len < 255) {
            inputText[len] = keys[col][0];
            inputText[len + 1] = '\0';
          }
        }
      }
    }
  }
}
