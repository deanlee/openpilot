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

Keyboard::Keyboard() {
  inputText[0] = '\0'; // Initialize the input text
}

void Keyboard::render(const Rectangle &rect) {
  // Draw the input box
  GuiTextBox((Rectangle){rect.x, rect.y, rect.width, 30}, inputText, 256, true);

  // Draw the keyboard below the input box
  float keyWidth = rect.width / 10;
  float keyHeight = 30;
  const char *keys[] = {
    "1", "2", "3", "4", "5", "6", "7", "8", "9", "0",
    "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P",
    "A", "S", "D", "F", "G", "H", "J", "K", "L", ";",
    "Z", "X", "C", "V", "B", "N", "M", ",", ".", "/"
  };

  int keyIndex = 0;
  for (int row = 0; row < 4; row++) {
    for (int col = 0; col < 10; col++) {
      if (keyIndex >= sizeof(keys) / sizeof(keys[0])) break;

      Rectangle keyRect = {rect.x + col * keyWidth, rect.y + 40 + row * keyHeight, keyWidth, keyHeight};
      if (GuiButton(keyRect, keys[keyIndex])) {
        // Append the key to the input text
        int len = strlen(inputText);
        if (len < 255) {
          inputText[len] = keys[keyIndex][0];
          inputText[len + 1] = '\0';
        }
      }
      keyIndex++;
    }
  }
}
