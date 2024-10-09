#include <string>
#include "system/hardware/hw.h"
#include "third_party/raylib/include/raylib.h"

int main(int argc, char* argv[]) {
  InitWindow(0, 0, "Text");
  SetTargetFPS(20);

  const int margin = 50;
  const int fontSize = 60;
  const float screenWidth = GetScreenWidth();
  const float screenHeight = GetScreenHeight();
  const std::string text = argv[1];

  // Scrolling state
  float scrollY = 0;
  const float scrollSpeed = 20.0f;
  const Rectangle scrollArea = {margin, margin, screenWidth - margin * 2, screenHeight - margin * 2 - 100};

  // Reboot button settings
  std::string buttonText = "Reboot";
  const Rectangle rebootButton = {screenWidth / 2 - 50, screenHeight - 60, 100, 40};

  while (!WindowShouldClose()) {
    Vector2 mousePoint = GetMousePosition();
    if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mousePoint, rebootButton)) {
      Hardware::reboot();
    }

    float wheel = GetMouseWheelMove();
    scrollY -= wheel * scrollSpeed;

    BeginDrawing();
      ClearBackground(BLACK);

      BeginScissorMode(scrollArea.x, scrollArea.y, scrollArea.width, scrollArea.height);
      DrawText(text.c_str(), scrollArea.x, scrollArea.y - scrollY, fontSize, WHITE);
      EndScissorMode();

      DrawRectangleRec(rebootButton, GRAY);
      DrawText("Reboot", rebootButton.x + 10, rebootButton.y + 10, fontSize, WHITE);
    EndDrawing();
  }

  CloseWindow();
  return 0;
}
