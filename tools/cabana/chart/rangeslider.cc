#include "chat/rangeslider.h"

#include <QPainter>
#include <QStyle>
#include <QStyleOptionSlider>

RangeSlider::RangeSlider(QWidget *parent) : QWidget(parent) {
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

QSize RangeSlider::sizeHint() const {
  return {0, style()->pixelMetric(QStyle::PM_SliderThickness)};
}

void RangeSlider::paintEvent(QPaintEvent *event) {
  QPainter p(this);
  QStyleOptionSlider opt;
  opt.initFrom(this);
  opt.minimum = min_ * 1000.0;
  opt.maximum = max_ * 1000.0;
  opt.rect = rect();
  opt.sliderPosition = opt.minimum;
  opt.subControls = QStyle::SC_SliderGroove;
  opt.tickInterval = 1;
  // p.fillRect(rect(), Qt::red);
  style()->drawComplexControl(QStyle::CC_Slider, &opt, &p, this);

  auto groove_rect = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderGroove, this);
  QColor color = palette().color(QPalette::Normal, QPalette::Highlight);
  color.setAlpha(160);
  QRect rc = groove_rect;
  p.fillRect(rc.adjusted(50, 0, 0, 0), color);
}

void RangeSlider::mouseMoveEvent(QMouseEvent *event) {
}

void RangeSlider::mousePressEvent(QMouseEvent *event) {
}

void RangeSlider::mouseReleaseEvent(QMouseEvent *event) {
}
