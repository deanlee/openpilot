#include "system/ui/raylib/util.h"
#include "system/ui/raylib/controls/keyboard.h"
#include "third_party/raylib/include/raygui.h"

int main() {
  App app("Wi-Fi Manager", 30);

  Keyboard keyboard;;
  while (!WindowShouldClose()) {
    BeginDrawing();
    ClearBackground(RAYLIB_BLACK);
    keyboard.render({40, 100, GetScreenWidth() - 80.0f, GetScreenHeight() - 180.0f}, "Enter password", "Please enter password");
    EndDrawing();
  }
  return 0;
}
