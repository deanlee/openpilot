#include <algorithm>
#include <cmath>
#include <iostream>

#include "selfdrive/ui/raylib/util.h"
#include "third_party/raylib/include/raylib.h"

constexpr int kProgressBarWidth = 1000;
constexpr int kProgressBarHeight = 20;
constexpr float kRotationRate = 12.0f;
constexpr int kMargin = 200;
constexpr int kTextureSize = 360;
constexpr int kFontSize = 80;

int main(int argc, char *argv[]) {
  initApp("spinner", 30);

  // Turn off input buffering for std::cin
  std::cin.sync_with_stdio(false);
  std::cin.tie(nullptr);

  const int screenWidth = GetScreenWidth();
  const int screenHeight = GetScreenHeight();
  Vector2 screenCenter = {screenWidth / 2.0f, screenHeight / 2.0f};

  Texture2D commaTexture = LoadTextureResized("../assets/img_spinner_comma.png", kTextureSize);
  Texture2D spinnerTexture = LoadTextureResized("../assets/img_spinner_track.png", kTextureSize);

  const Vector2 spinnerOrigin{spinnerTexture.width / 2.0f, spinnerTexture.height / 2.0f};
  const Vector2 commaPosition{screenCenter.x - commaTexture.width / 2.0f, screenCenter.y - commaTexture.height / 2.0f};

  float rotationAngle = 0.0f;
  std::string userInput;

  while (!WindowShouldClose()) {
    rotationAngle = fmod(rotationAngle + kRotationRate, 360.0f);

    BeginDrawing();
    ClearBackground(BLACK);

    // Draw rotating spinner and static comma logo
    DrawTexturePro(spinnerTexture, {0, 0, (float)spinnerTexture.width, (float)spinnerTexture.height},
                   {screenCenter.x, screenCenter.y, (float)spinnerTexture.width, (float)spinnerTexture.height},
                   spinnerOrigin, rotationAngle, WHITE);
    DrawTextureV(commaTexture, commaPosition, WHITE);

    // Check for user input
    if (std::cin.rdbuf()->in_avail() > 0) {
      std::getline(std::cin, userInput);
    }

    // Display either a progress bar or user input text based on input
    if (!userInput.empty()) {
      float yPos = screenHeight - kMargin - kProgressBarHeight;
      if (std::all_of(userInput.begin(), userInput.end(), ::isdigit)) {
        int progressValue = std::clamp(std::stoi(userInput), 0, 100);
        Rectangle progressBar = {screenCenter.x - kProgressBarWidth / 2.0f, yPos, kProgressBarWidth, kProgressBarHeight};
        DrawRectangleRounded(progressBar, 0.5f, 10, GRAY);
        DrawRectangleRounded({progressBar.x, progressBar.y, progressBar.width * (progressValue / 100.0f), progressBar.height}, 0.5f, 10, RAYWHITE);
      } else {
        Vector2 textSize = MeasureTextEx(getFont(), userInput.c_str(), kFontSize, 1.0);
        DrawTextEx(getFont(), userInput.c_str(), {screenCenter.x - textSize.x / 2, yPos}, kFontSize, 1.0, WHITE);
      }
    }

    EndDrawing();
  }

  CloseWindow();
  return 0;
}
