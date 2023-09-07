#include "tools/cabana/common/toolbutton.h"

#include <QApplication>

#include "tools/cabana/settings.h"
#include "tools/cabana/common/util.h"

ToolButton::ToolButton(const QString &icon, const QString &tooltip, QWidget *parent) : QToolButton(parent) {
  setIcon(icon);
  setToolTip(tooltip);
  setAutoRaise(true);
  const int metric = QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize);
  setIconSize({metric, metric});
  theme = settings.theme;
  connect(&settings, &Settings::changed, this, &ToolButton::updateIcon);
}

void ToolButton::setIcon(const QString &icon) {
  icon_str = icon;
  QToolButton::setIcon(utils::icon(icon_str));
}

void ToolButton::updateIcon() {
  if (std::exchange(theme, settings.theme) != theme) {
    setIcon(icon_str);
  }
}
