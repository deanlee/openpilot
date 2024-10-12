#pragma once

#include <QPainter>
#include "selfdrive/ui/ui.h"

const int btn_size = 192;
const int img_size = (btn_size / 4) * 3;

class ExperimentalButton {
public:
  explicit ExperimentalButton();
  void updateState(const UIState &s);

private:
  void draw(QPainter &painer, const QRect &surface_rect);
  void changeMode();

  Params params;
  QPixmap engage_img;
  QPixmap experimental_img;
  bool experimental_mode;
  bool engageable;
  bool is_down = false;
};

void drawIcon(QPainter &p, const QPoint &center, const QPixmap &img, const QBrush &bg, float opacity);
