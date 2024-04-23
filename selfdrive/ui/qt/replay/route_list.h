#pragma once

#include <QWidget>

#include "selfdrive/ui/qt/onroad/onroad_home.h"

class RouteList : public QWidget {
  Q_OBJECT

public:
  RouteList(QWidget *parent = nullptr);

};

class MainWin : public QWidget {
  Q_OBJECT

public:
  MainWin(QWidget *parent = nullptr);

  OnroadWindow *video_widget;
};
