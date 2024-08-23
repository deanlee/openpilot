#include "selfdrive/ui/qt/onroad/onroad_home.h"

#include <raylib.h>

OnroadWindow::OnroadWindow(QWidget *parent) : QWidget(parent) {
  setAttribute(Qt::WA_TranslucentBackground, true);
  SetConfigFlags(FLAG_WINDOW_UNDECORATED | FLAG_WINDOW_HIDDEN | FLAG_MSAA_4X_HINT | FLAG_BORDERLESS_WINDOWED_MODE);
  InitWindow(width(), height(), "");
  auto winid = winId();
  embedRaylibInQtWidget(&winid, GetWindowHandle());

  QObject::connect(uiState(), &UIState::uiUpdate, this, &OnroadWindow::updateState);
  QObject::connect(uiState(), &UIState::offroadTransition, this, &OnroadWindow::offroadTransition);

  camera = std::make_unique<AnnotatedCamera>();
  // QObject::connect(nvg, &AnnotatedCameraWidget::clicked, this, &OnroadWindow::clicked);
}

OnroadWindow::~OnroadWindow() {
  CloseWindow();
}

void OnroadWindow::updateState(const UIState &s) {
  if (!s.scene.started) {
    return;
  }
  if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
    emit clicked();
  }
  QColor bgColor = bg_colors[s.status];
  if (bg != bgColor) {
    // repaint border
    bg = bgColor;
  }

  camera->updateState(s);
  BeginDrawing();
  ClearBackground(RAYWHITE);
  camera->draw();
  EndDrawing();

  update();
}

void OnroadWindow::offroadTransition(bool offroad) {
  // alerts->clear();
}

void OnroadWindow::showEvent(QShowEvent *event) {
  QRect rect = geometry();
  QPoint pt = mapToGlobal(rect.topLeft());
  // TODO only resize if pt/rect changed
  SetWindowSize(rect.width(), rect.height());
  SetWindowPosition(pt.x(), pt.y());
  ClearWindowState(FLAG_WINDOW_HIDDEN);
   auto winid = winId();
  embedRaylibInQtWidget(&winid, GetWindowHandle());
  camera->draw();
}

void OnroadWindow::hideEvent(QHideEvent *event) {
  SetWindowState(FLAG_WINDOW_HIDDEN);
}

void OnroadWindow::resizeEvent(QResizeEvent *event) {
  QRect rect = geometry();
  QPoint pt = mapToGlobal(rect.topLeft());
  SetWindowSize(rect.width(), rect.height());
  SetWindowPosition(pt.x(), pt.y());
  camera->draw();
}
