#pragma once

#include <string>
#include <vector>
#include "system/ui/raylib/raylib.h"

class Keyboard {
public:
  Keyboard();
  void render(const Rectangle &rect, const std::string &title, const std::string &sub_title);
  char inputText[256] = {};
  std::vector<std::vector<std::string>> *layout;
};
