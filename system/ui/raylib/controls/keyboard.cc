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
  GuiSetFont(pApp->getFont());
  GuiSetStyle(DEFAULT, TEXT_SIZE, 40);
}

void Keyboard::render(const Rectangle &rect) {
  GuiTextBox((Rectangle){rect.x, rect.y, rect.width, 30}, inputText, 256, true);

  // Draw the keyboard below the input box
  // float keyWidth = rect.width / 10;
  float keyHeight = 155;

  // float h_space = 10;
  float v_space = 10;
  int keyIndex = 0;

  for (int row = 0; row < lowercase.size(); row++) {
    const auto &keys = lowercase[row];
    int start_x = row == 1 ? rect.x + 90 : rect.x;
    float key_width = row == 1 ? (rect.width - 180) / keys.size() : (rect.width) / keys.size();
    for (int col = 0; col < keys.size(); col++) {
      Rectangle keyRect = {start_x + col * key_width, rect.y + 40 + row * (keyHeight + v_space), key_width, keyHeight};
      if (GuiButton(keyRect, keys[col].c_str())) {
        int len = strlen(inputText);
        if (len < 255) {
          inputText[len] = keys[col][0];
          inputText[len + 1] = '\0';
        }
      }
      keyIndex++;
    }
  }
}
