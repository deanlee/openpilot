#pragma once

#include <QHBoxLayout>
#include <QWidget>
#include "selfdrive/ui/qt/onroad/annotated_camera.h"

class OnroadWindow : public QWidget {
  Q_OBJECT

public:
  OnroadWindow(QWidget* parent = 0);
  ~OnroadWindow();

signals:
  void clicked();

private:
  void showEvent(QShowEvent *event) override;
  void hideEvent(QHideEvent *event) override;
  void resizeEvent(QResizeEvent* event) override;

  std::unique_ptr<AnnotatedCamera> camera;
  QColor bg = bg_colors[STATUS_DISENGAGED];
  QHBoxLayout* split;

private slots:
  void offroadTransition(bool offroad);
  void updateState(const UIState &s);
};

void embedRaylibInQtWidget(void *wid, void *window);