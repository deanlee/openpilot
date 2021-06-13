
#include "selfdrive/ui/qt/widgets/control_list.h"

#include <QPainter>
#include <QScroller>
#include <QStyledItemDelegate>
#include <QVBoxLayout>

class ItemDelegate : public QStyledItemDelegate {
public:
  explicit ItemDelegate(QObject *parent = 0) : QStyledItemDelegate(parent) {}
  void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const {
    QPen oldPen = painter->pen();
    painter->setPen(Qt::gray);
    if (index.row() < index.model()->rowCount() - 1) {
      int b = option.rect.bottom() - 1;
      painter->drawLine(option.rect.left() + 40, b, option.rect.right() - 40, b);
    }
    painter->setPen(oldPen);

    QStyledItemDelegate::paint(painter, option, index);
  }

  QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override {
    ControlListWidget *p = (ControlListWidget *)parent();
    QListWidgetItem *item = p->listWidget()->item(index.row());
    QSize size = p->listWidget()->itemWidget(item)->sizeHint();
    size.setWidth(0);
    size.setHeight(size.height() + p->spacing() * 2);
    item->setSizeHint(size);
    return size;
  }
};

ControlListWidget::ControlListWidget(QWidget *parent) : QWidget(parent) {
  QVBoxLayout *main_layout = new QVBoxLayout(this);
  listWidget_ = new QListWidget(this);
  listWidget_->setItemDelegate(new ItemDelegate(this));
  listWidget_->setSizeAdjustPolicy(QListWidget::AdjustToContents);
  listWidget_->setResizeMode(QListView::Adjust);
  listWidget_->QAbstractScrollArea::setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  listWidget_->QAbstractScrollArea::setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  QScroller::grabGesture(listWidget_, QScroller::LeftMouseButtonGesture);
  
  main_layout->addWidget(listWidget_);

  setStyleSheet(R"(
    QListWidget {
      padding: 0px 0px 0px 0px;
    }
    QListWidget::item {
      padding-left:10px;
      padding-right:10px;
    }
  )");
}

void ControlListWidget::addWidget(QWidget *w) {
  QListWidgetItem *item = new QListWidgetItem(listWidget_);
  item->setFlags(Qt::NoItemFlags);
  item->setSizeHint(w->sizeHint());
  listWidget_->addItem(item);
  listWidget_->setItemWidget(item, w);
}
