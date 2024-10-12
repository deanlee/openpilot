#include "selfdrive/ui/qt/onroad/buttons.h"
#include <QApplication>
#include "selfdrive/ui/qt/util.h"

void drawIcon(QPainter &p, const QPoint &center, const QPixmap &img, const QBrush &bg, float opacity) {
  p.setRenderHint(QPainter::Antialiasing);
  p.setOpacity(1.0);  // bg dictates opacity of ellipse
  p.setPen(Qt::NoPen);
  p.setBrush(bg);
  p.drawEllipse(center, btn_size / 2, btn_size / 2);
  p.setOpacity(opacity);
  p.drawPixmap(center - QPoint(img.width() / 2, img.height() / 2), img);
  p.setOpacity(1.0);
}

ExperimentalButton::ExperimentalButton() : experimental_mode(false), engageable(false) {
  engage_img = loadPixmap("../assets/img_chffr_wheel.png", {img_size, img_size});
  experimental_img = loadPixmap("../assets/img_experimental.svg", {img_size, img_size});
}

void ExperimentalButton::changeMode() {
  const auto cp = (*uiState()->sm)["carParams"].getCarParams();
  bool can_change = hasLongitudinalControl(cp) && params.getBool("ExperimentalModeConfirmed");
  if (can_change) {
    params.putBool("ExperimentalMode", !experimental_mode);
  }
}

void ExperimentalButton::updateState(const UIState &s) {
  const auto cs = (*s.sm)["selfdriveState"].getSelfdriveState();
  bool eng = cs.getEngageable() || cs.getEnabled();
  if ((cs.getExperimentalMode() != experimental_mode) || (eng != engageable)) {
    engageable = eng;
    experimental_mode = cs.getExperimentalMode();
  }
}

void ExperimentalButton::draw(QPainter &painter, const QRect &surface_rect) {
  QRect rect(surface_rect.right() - btn_size - UI_BORDER_SIZE, UI_BORDER_SIZE, btn_size, btn_size);
  bool left_button_down = QGuiApplication::mouseButtons() & Qt::LeftButton;
  if (!left_button_down && is_down && rect.contains(QCursor::pos())) {
    changeMode();
  }
  is_down = left_button_down;
  const QPixmap &img = experimental_mode ? experimental_img : engage_img;
  drawIcon(painter, rect.center(), img, QColor(0, 0, 0, 166), (is_down || !engageable) ? 0.6 : 1.0);
}
