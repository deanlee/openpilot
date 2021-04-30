#pragma once

#include <QOpenGLFunctions>
#include <QOpenGLWidget>
#include <QTimer>

#include "ui/ui.hpp"

// container window for the NVG UI
class DriverView : public QOpenGLWidget, protected QOpenGLFunctions {
  Q_OBJECT

public:
  using QOpenGLWidget::QOpenGLWidget;
  explicit DriverView(QWidget* parent = 0) : QOpenGLWidget(parent) {};
  ~DriverView();

protected:
  void paintGL() override;
  void initializeGL() override;

private:
QTimer *timer;
  double prev_draw_t = 0;
  std::unique_ptr<UIVision> vision;

public slots:
  void update();
};
