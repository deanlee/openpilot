#pragma once
#include <string>
#include <raylib.h>

enum class FontWeight {
  Normal,
  Bold,
  ExtraBold,
  ExtraLight,
  Medium,
  Regular,
  SemiBold,
  Thin,
  Count // To represent the total number of fonts
};


void drawCenteredText(Rectangle rec, const std::string &text, float font_size, const Color &color, FontWeight weight = FontWeight::Normal);
void drawHCenterText(Rectangle &rec, const std::string &text, float font_size, const Color &color, FontWeight weight = FontWeight::Normal);
void draw_text(const std::string &text, float x, float y, float font_size, const Color &color, FontWeight weight = FontWeight::Normal);
Texture2D LoadTextureResized(const char *fileName, int size);
void drawCircleTexture(Texture2D *texture, int x, int y, uint8_t opacity);

