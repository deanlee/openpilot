#include <vector>

#include "selfdrive/ui/raylib/util.h"
#include "third_party/raylib/include/raylib.h"
void DrawTextLines(const std::string &text, float startX, float startY, float scrollX, float scrollY, int fontSize, Color color) {
  float yPos = startY - scrollY;
  float xPos = startX - scrollX;
  DrawTextEx(getFont(), text.c_str(), {xPos, yPos}, fontSize, 1.0, color);
}

int guiButton(Rectangle bounds, const char *text) {
  int result = 0;
  Vector2 mousePoint = GetMousePosition();
  // Check button state
  if (CheckCollisionPointRec(mousePoint, bounds)) {
    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {

    }

    if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) result = 1;
  }
  return result;
}

int main(int argc, char* argv[]) {
  initApp("Text", 20);

  const float screenWidth = GetScreenWidth();
  const float screenHeight = GetScreenHeight();

  // Set up font size, line height, and text data
  const int fontSize = 60;
  std::string text = R"(
      "This is a scrollable text area.",
      "Scroll with the mouse wheel or keyboard.",
      "Each line of text is drawn separately.",
      "You can customize the content here.",
      "Raylib makes it easy to handle drawing.",
      "Handle scrolling manually for large text blocks.",
      "This example demonstrates simple scrolling.",
      "Have fun experimenting with Raylib!",
      "Line 9",
      "Line 10",
      "Line 11",
      "Line 12",
      "Line 13",
      "Line 14",
      "Line 15",
      "Line 16",
      "Line 17",
      "Line 18",
      "Line 19",
      "Line 20)";

  // Scrolling state
  float scrollY = 0;
  float scrollX = 0;
  const int margin = 50;
  const float scrollSpeed = 20.0f;

  // Scrollable area dimensions
  const Rectangle scrollArea = {margin, margin, screenWidth - margin * 2, screenHeight - margin * 2 - 100};

  // Variables for handling mouse drag scrolling
  bool dragging = false;
  Vector2 previousMousePos = {0, 0};

  // Reboot button settings
  std::string buttonText = "Reboot";

  const Rectangle rebootButton = {screenWidth / 2 - 50, screenHeight - 60, 100, 40};
  bool buttonHovered = false;

  Vector2 textSize = MeasureTextEx(getFont(), text.c_str(), fontSize, 1.0);
  float maxScrollX = std::max<float>(0, textSize.x - scrollArea.width);
  float maxScrollY = textSize.y - scrollArea.height;

  // Main loop
  while (!WindowShouldClose()) {
    // Handle scrolling with mouse wheel (vertical)
    float wheel = GetMouseWheelMove();
    scrollY -= wheel * scrollSpeed;

    // Handle dragging with left mouse button or touch (vertical and horizontal)
    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
      Vector2 currentMousePos = GetMousePosition();
      if (!dragging) {
        dragging = true;
      } else {
        scrollX -= (currentMousePos.x - previousMousePos.x);
        scrollY -= (currentMousePos.y - previousMousePos.y);
      }
      buttonHovered = CheckCollisionPointRec(currentMousePos, rebootButton);
      previousMousePos = currentMousePos;
    } else {
      dragging = false;
      buttonHovered = false;
    }

    // Ensure scrolling stays within bounds
    if (scrollY < 0) scrollY = 0;
    if (scrollY > maxScrollY) scrollY = maxScrollY;

    // Adjust horizontal scrolling (optional)
    if (scrollX < 0) scrollX = 0;
    if (scrollX > maxScrollX) scrollX = maxScrollX;

    // Handle click on the reboot button
    if (buttonHovered && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
      // Example: system("sudo reboot"); -- Linux command to reboot (requires sudo permission)
      DrawText("Rebooting...", 10, 10, 20, RED);
    }

    BeginDrawing();
    ClearBackground(BLACK);

    BeginScissorMode(scrollArea.x, scrollArea.y, scrollArea.width, scrollArea.height);
    DrawTextLines(text, scrollArea.x, scrollArea.y, scrollX, scrollY, fontSize, WHITE);
    EndScissorMode();

    // Draw the reboot button
    Color buttonColor = buttonHovered ? DARKGRAY : GRAY;
    DrawRectangleRec(rebootButton, buttonColor);
    DrawTextEx(getFont(), "Reboot", {rebootButton.x + 10, rebootButton.y + 10}, fontSize, 1.0, WHITE);

    EndDrawing();
  }

  CloseWindow();
  return 0;
}
