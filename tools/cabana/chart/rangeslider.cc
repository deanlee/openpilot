#include "tools/cabana/chart/rangeslider.h"

#include <QApplication>
#include <QPainter>
#include <QStyle>


RangeSlider::RangeSlider(QWidget *parent) : QWidget(parent) {
  setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed, QSizePolicy::Slider));
  // opt.tickInterval = 1;
}

QSize RangeSlider::sizeHint() const {
  int h = 50;//style()->pixelMetric(QStyle::PM_SliderThickness, &opt);

  return style()->sizeFromContents(QStyle::CT_Slider, &opt, QSize(rect().width(), h), this)
      .expandedTo(QApplication::globalStrut());
}

void RangeSlider::paintEvent(QPaintEvent *event) {
  QPainter p(this);
  opt.initFrom(this);
  opt.minimum =0;//min_ * 1000.0;
  opt.maximum = 0;//max_ * 1000.0;
  opt.rect = rect();
  opt.sliderPosition = 0;//100*1000.0;//opt.minimum;
  opt.subControls = QStyle::SC_SliderGroove;// |  QStyle::SC_SliderTickmarks;
  // p.fillRect(rect(), Qt::red);
  style()->drawComplexControl(QStyle::CC_Slider, &opt, &p);

  auto groove_rect = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderGroove);
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
