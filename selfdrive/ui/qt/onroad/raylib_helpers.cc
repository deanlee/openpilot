#include "selfdrive/ui/qt/onroad/raylib_helpers.h"

#include <raylib.h>

#include <array>

constexpr std::array<const char *, static_cast<int>(FontWeight::Count)> FONT_FILE_PATHS = {
    "../assets/fonts/Inter-Black.ttf",
    "../assets/fonts/Inter-Bold.ttf",
    "../assets/fonts/Inter-ExtraBold.ttf",
    "../assets/fonts/Inter-ExtraLight.ttf",
    "../assets/fonts/Inter-Medium.ttf",
    "../assets/fonts/Inter-Regular.ttf",
    "../assets/fonts/Inter-SemiBold.ttf",
    "../assets/fonts/Inter-Thin.ttf",
};

struct FontManager {
  FontManager() {
    for (int i = 0; i < fonts.size(); ++i) {
      fonts[i] = LoadFontEx(FONT_FILE_PATHS[i], 120, 0, 250);
      SetTextureFilter(fonts[i].texture, TEXTURE_FILTER_TRILINEAR);
    }
  }

  ~FontManager() {
    for (auto &f : fonts) {
      UnloadFont(f);
    }
  }
  std::array<Font, static_cast<int>(FontWeight::Count)> fonts;
};

Font get_font(FontWeight weight) {
  static FontManager font_manager;
  return font_manager.fonts[(int)weight];
}

void drawCenteredText(Rectangle rec, const std::string &text, float font_size, const Color &color, FontWeight weight) {
  // Get the default font and measure text size
  Font font = get_font(weight);
  Vector2 text_size = MeasureTextEx(font, text.c_str(), font_size, 1.0f);

  // Calculate position to center text in the rectangle
  float x = rec.x + (int)((rec.width - text_size.x) / 2);
  float y = rec.y + (int)((rec.height - text_size.y) / 2);

  // Draw the text
  DrawTextEx(font, text.c_str(), (Vector2){x, y}, font_size, 1.0f, color);
}

void drawHCenterText(Rectangle &rec, const std::string &text, float font_size, const Color &color, FontWeight weight) {
  int text_width = MeasureText(text.c_str(), font_size);
  DrawTextEx(get_font(weight), text.c_str(), (Vector2){rec.x + (rec.width - text_width) / 2, rec.y}, font_size, 1.0f, color);
}

void draw_text(const std::string &text, float x, float y, float font_size, const Color &color, FontWeight weight) {
  DrawTextEx(get_font(weight), text.c_str(), (Vector2){x, y}, font_size, 1.0f, color);
}
