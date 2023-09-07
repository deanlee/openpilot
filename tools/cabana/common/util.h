#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QPixmap>
#include <QString>
#include <cmath>
#include <deque>
#include <utility>
#include <vector>

#include "tools/cabana/dbc/dbc.h"

namespace utils {

inline QString toHex(const QByteArray &dat) { return dat.toHex(' ').toUpper(); }
QString toHex(uint8_t byte);
QPixmap icon(const QString &id);
void setTheme(int theme);
int num_decimals(double num);
QString signalToolTip(const cabana::Signal *sig);
inline QString formatSeconds(int seconds) {
  return QDateTime::fromSecsSinceEpoch(seconds, Qt::UTC).toString(seconds > 60 * 60 ? "hh:mm:ss" : "mm:ss");
}

}  // namespace utils
