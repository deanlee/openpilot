#pragma once

#include "system/ui/raylib/raylib.h"

class Keyboard {
public:
  Keyboard();
  void render(const Rectangle &rect);
  char inputText[256] = {};
};
