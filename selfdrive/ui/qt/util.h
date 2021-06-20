#pragma once

#include <QString>

#include "selfdrive/common/params.h"

class QDateTime;
class QLayout;
class QPainter;

inline QString getBrand() {
  return Params().getBool("Passive") ? "dashcam" : "openpilot";
}

inline QString getBrandVersion() {
  return getBrand() + " v" + QString::fromStdString(Params().get("Version")).left(14).trimmed();
}

void clearLayout(QLayout* layout);
void configFont(QPainter &p, const QString &family, int size, const QString &style);
void setQtSurfaceFormat();
QString timeAgo(const QDateTime &date);
