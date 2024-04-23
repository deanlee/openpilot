#include "selfdrive/ui/qt/replay/route_list.h"

#include <QHBoxLayout>

RouteList::RouteList(QWidget *parent) : QWidget(parent) {
  setVisible(true);
  showMaximized();
}


MainWin::MainWin(QWidget *parent) : QWidget(parent) {
  QHBoxLayout *main_layout = new QHBoxLayout(this);
  main_layout->addWidget(video_widget = new OnroadWindow(this));
  showMaximized();
}

