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
  DrawText(title.c_str(), rect.x, rect.y, 40, RAYLIB_WHITE);
  DrawText(sub_title.c_str(), rect.x, rect.y + 45, 20, RAYLIB_GRAY);

  GuiTextBox((Rectangle){rect.x, rect.y + 75, rect.width, 30}, inputText, 256, true);

  float keyHeight = 155;
  float v_space = 3;
  float key_max_width = rect.width / layout->at(2).size();

  for (int row = 0; row < layout->size(); row++) {
    const auto &keys = layout->at(row);
    float key_width = std::min((rect.width - (row == 1 ? 180 : 0)) / keys.size(), key_max_width);
    int start_x = rect.x + (row == 1 ? 90 : 0);

    for (const auto &key : keys) {
      int new_width = (key == "  ") ? key_width * 3 : (key == ENTER_KEY ? key_width * 2 : key_width);
      Rectangle keyRect = {(float)start_x, rect.y + 115 + row * (keyHeight + v_space), (float)new_width, keyHeight};
      start_x += new_width;

      if (GuiButton(keyRect, key.c_str())) {
        handleKeyPress(key);
      }
    }
  }
}

void Keyboard::handleKeyPress(const std::string &key) {
  if (key == "↓" || key == "ABC") {
    layout = &lowercase;
  } else if (key == "↑") {
    layout = &uppercase;
  } else if (key == "123") {
    layout = &numbers;
  } else if (key == "#+=") {
    layout = &specials;
  } else if (key == BACKSPACE_KEY && strlen(inputText) > 0) {
    inputText[strlen(inputText) - 1] = '\0';
  } else if (key != BACKSPACE_KEY && strlen(inputText) < 255) {
    inputText[strlen(inputText)] = key[0];
    inputText[strlen(inputText) + 1] = '\0';
  }
}
