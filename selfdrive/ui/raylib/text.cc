#include <vector>

#include "selfdrive/ui/raylib/util.h"
#include "third_party/raylib/include/raylib.h"
void DrawTextLines(const std::string &text, float startX, float startY, float scrollX, float scrollY, int fontSize, Color color) {
  float yPos = startY - scrollY;
  float xPos = startX - scrollX;
  // for (const auto& line : lines) {
  // DrawText(text.c_str(), 10, yPos, fontSize, color);
  DrawTextEx(getFont(), text.c_str(), {xPos, yPos}, fontSize, 1.0, color);
  // yPos += lineHeight;
  // }
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
  const float scrollSpeed = 20.0f;

  // Scrollable area dimensions
  const Rectangle scrollArea = {50, 50, screenWidth - 100, screenHeight - 100 - 100};

  // Variables for handling mouse drag scrolling
  bool dragging = false;
  Vector2 previousMousePos = {0, 0};

  // Reboot button settings
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
      if (!dragging) {
        dragging = true;
        previousMousePos = GetMousePosition();
      } else {
        Vector2 currentMousePos = GetMousePosition();
        float deltaX = currentMousePos.x - previousMousePos.x;  // Horizontal drag
        float deltaY = currentMousePos.y - previousMousePos.y;  // Vertical drag
        scrollX -= deltaX;
        scrollY -= deltaY;
        previousMousePos = currentMousePos;
      }
    } else {
      dragging = false;
    }

    // Ensure scrolling stays within bounds
    if (scrollY < 0) scrollY = 0;
    if (scrollY > maxScrollY) scrollY = maxScrollY;

    // Adjust horizontal scrolling (optional)
    if (scrollX < 0) scrollX = 0;
    if (scrollX > maxScrollX) scrollX = maxScrollX;

    // Check if the mouse is over the reboot button
    Vector2 mousePos = GetMousePosition();
    buttonHovered = CheckCollisionPointRec(mousePos, rebootButton);

    // Handle click on the reboot button
    if (buttonHovered && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
      // Example: system("sudo reboot"); -- Linux command to reboot (requires sudo permission)
      DrawText("Rebooting...", 10, 10, 20, RED);
    }

    BeginDrawing();
    ClearBackground(RAYWHITE);

    // Draw scrollable area background
    DrawRectangleRec(scrollArea, LIGHTGRAY);

    // Limit drawing to the scrollable area using scissor mode
    BeginScissorMode(scrollArea.x, scrollArea.y, scrollArea.width, scrollArea.height);

    // Draw the text lines within the scrollable area
    DrawTextLines(text, scrollArea.x, scrollArea.y, scrollX, scrollY, fontSize, BLACK);

    EndScissorMode();

    // Draw a border around the scroll area
    DrawRectangleLinesEx(scrollArea, 2, DARKGRAY);

    // Draw the reboot button
    Color buttonColor = buttonHovered ? DARKGRAY : GRAY;
    DrawRectangleRec(rebootButton, buttonColor);
    DrawText("Reboot", rebootButton.x + 10, rebootButton.y + 10, 20, WHITE);

    EndDrawing();
  }

  CloseWindow();
  return 0;
}
