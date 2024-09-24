#pragma once

const int btn_size = 192;
const int img_size = (btn_size / 4) * 3;

void drawIcon(QPainter &p, const QPoint &center, const QPixmap &img, const QBrush &bg, float opacity);
