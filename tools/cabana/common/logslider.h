#pragma once

#include <QSlider>

class LogSlider : public QSlider {
  Q_OBJECT

public:
  LogSlider(double factor, Qt::Orientation orientation, QWidget *parent = nullptr)
      : factor(factor), QSlider(orientation, parent) {}
  void setRange(double min, double max);
  int value() const;
  void setValue(int v);

private:
  double factor, log_min = 0, log_max = 1;
};
