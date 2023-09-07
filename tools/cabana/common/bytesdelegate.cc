#include "tools/cabana/common/bytesdelegate.h"

#include <QApplication>
#include <QFontDatabase>
#include <QPainter>
#include <algorithm>

#include "tools/cabana/common/util.h"

MessageBytesDelegate::MessageBytesDelegate(QObject *parent, bool multiple_lines) : multiple_lines(multiple_lines), QStyledItemDelegate(parent) {
  fixed_font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
  byte_size = QFontMetrics(fixed_font).size(Qt::TextSingleLine, "00 ") + QSize(0, 2);
}

int MessageBytesDelegate::widthForBytes(int n) const {
  int h_margin = QApplication::style()->pixelMetric(QStyle::PM_FocusFrameHMargin) + 1;
  return n * byte_size.width() + h_margin * 2;
}

QSize MessageBytesDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const {
  int v_margin = QApplication::style()->pixelMetric(QStyle::PM_FocusFrameVMargin) + 1;
  auto data = index.data(BytesRole);
  if (!data.isValid()) {
    return {1, byte_size.height() + 2 * v_margin};
  }
  int n = data.toByteArray().size();
  assert(n >= 0 && n <= 64);
  return !multiple_lines ? QSize{widthForBytes(n), byte_size.height() + 2 * v_margin}
                         : QSize{widthForBytes(8), byte_size.height() * std::max(1, n / 8) + 2 * v_margin};
}

void MessageBytesDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const {
  auto data = index.data(BytesRole);
  if (!data.isValid()) {
    return QStyledItemDelegate::paint(painter, option, index);
  }

  auto byte_list = data.toByteArray();
  auto colors = index.data(ColorsRole).value<QVector<QColor>>();

  int v_margin = option.widget->style()->pixelMetric(QStyle::PM_FocusFrameVMargin);
  int h_margin = option.widget->style()->pixelMetric(QStyle::PM_FocusFrameHMargin);
  if (option.state & QStyle::State_Selected) {
    painter->fillRect(option.rect, option.palette.brush(QPalette::Normal, QPalette::Highlight));
  }

  const QPoint pt{option.rect.left() + h_margin, option.rect.top() + v_margin};
  QFont old_font = painter->font();
  QPen old_pen = painter->pen();
  painter->setFont(fixed_font);
  for (int i = 0; i < byte_list.size(); ++i) {
    int row = !multiple_lines ? 0 : i / 8;
    int column = !multiple_lines ? i : i % 8;
    QRect r = QRect({pt.x() + column * byte_size.width(), pt.y() + row * byte_size.height()}, byte_size);
    if (i < colors.size() && colors[i].alpha() > 0) {
      if (option.state & QStyle::State_Selected) {
        painter->setPen(option.palette.color(QPalette::Text));
        painter->fillRect(r, option.palette.color(QPalette::Window));
      }
      painter->fillRect(r, colors[i]);
    } else if (option.state & QStyle::State_Selected) {
      painter->setPen(option.palette.color(QPalette::HighlightedText));
    }
    painter->drawText(r, Qt::AlignCenter, utils::toHex(byte_list[i]));
  }
  painter->setFont(old_font);
  painter->setPen(old_pen);
}
