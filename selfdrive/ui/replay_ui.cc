#include <QApplication>
#include <QCommandLineParser>

#include "selfdrive/ui/qt/util.h"
#include "selfdrive/ui/qt/replay/route_list.h"

int main(int argc, char *argv[]) {
  QCoreApplication::setApplicationName("Replay");
  QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
  initApp(argc, argv, false);
  QApplication app(argc, argv);
  app.setApplicationDisplayName("Replay");

  MainWin main_win;
  return app.exec();
}
