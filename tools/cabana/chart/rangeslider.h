#pragma once

#include <QWidget>
#include <QStyleOptionSlider>
class RangeSlider : public QWidget {
  Q_OBJECT
public:
  RangeSlider(QWidget *parent);
  QSize sizeHint() const override;
  void setRange(double min, double max) {
    min_ = min;
    max_ = max;
    update();
  }
  void setZoomRange(double min, double max) {
    zoom_min_ = min;
    zoom_max_ = max;
    update();
  }
protected:
  void paintEvent(QPaintEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;

  double min_ = 0;
  QStyleOptionSlider opt;
  double max_ = 0;
  double zoom_min_ = 0;
  double zoom_max_ = 0;
};
