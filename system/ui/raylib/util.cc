#include "system/ui/raylib/util.h"

#include <array>

#undef GREEN
#undef RED
#undef YELLOW
#include "common/swaglog.h"
#include "system/hardware/hw.h"

constexpr std::array<const char *, static_cast<int>(FontWeight::Count)> FONT_FILE_PATHS = {
    "../../assets/fonts/Inter-Black.ttf",
    "../../assets/fonts/Inter-Bold.ttf",
    "../../assets/fonts/Inter-ExtraBold.ttf",
    "../../assets/fonts/Inter-ExtraLight.ttf",
    "../../assets/fonts/Inter-Medium.ttf",
    "../../assets/fonts/Inter-Regular.ttf",
    "../../assets/fonts/Inter-SemiBold.ttf",
    "../../assets/fonts/Inter-Thin.ttf",
};

struct FontManager {
  FontManager() {
    for (int i = 0; i < fonts.size(); ++i) {
      fonts[i] = LoadFontEx(FONT_FILE_PATHS[i], 120, nullptr, 250);
    }
  }

  ~FontManager() {
    for (auto &f : fonts) UnloadFont(f);
  }

  std::array<Font, static_cast<int>(FontWeight::Count)> fonts;
};

const Font& getFont(FontWeight weight) {
  static FontManager font_manager;
  return font_manager.fonts[(int)weight];
}

Texture2D LoadTextureResized(const char *fileName, int size) {
  Image img = LoadImage(fileName);
  ImageResize(&img, size, size);
  Texture2D texture = LoadTextureFromImage(img);
  return texture;
}
#include <cassert>
void initApp(const char *title, int fps) {
  Hardware::set_display_power(true);
  Hardware::set_brightness(65);


  // auto r = setlocale(LC_ALL, "zh_CN.UTF-8");
  // assert(r);
  // printf("%s \n", r);
  setlocale(LC_ALL, "zh_CN.UTF-8");
  // assert(setlocale(LC_ALL, "zh_CN.UTF-8") != nullptr);
  auto result = bindtextdomain("main", "/home/dean/Projects/openpilot/system/ui/raylib/translations");
  assert(result);
  printf("bindtextdomain %s\n", result);
  auto dd = textdomain("main");
  assert(dd);
  setlocale(LC_ALL, "zh_CN.UTF-8");
  const char* translated_str = gettext("spinner");
  printf("Translated text: 我的 %s\n", translated_str);


  SetTraceLogLevel(LOG_NONE);
  InitWindow(0, 0, title);
  SetTargetFPS(fps);
}
