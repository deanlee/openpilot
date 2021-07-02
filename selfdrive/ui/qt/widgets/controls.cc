#include "selfdrive/ui/qt/widgets/controls.h"

#include <QDebug>
#include <QStyledItemDelegate>
#include <QScrollBar>
#include <QScroller>
#include <QVariant>

QFrame *horizontal_line(QWidget *parent) {
  QFrame *line = new QFrame(parent);
  line->setFrameShape(QFrame::StyledPanel);
  line->setStyleSheet(R"(
    margin-left: 40px;
    margin-right: 40px;
    border-width: 1px;
    border-bottom-style: solid;
    border-color: gray;
  )");
  line->setFixedHeight(2);
  return line;
}

AbstractControl::AbstractControl(const QString &title, const QString &desc, const QString &icon, QWidget *parent) : QFrame(parent) {
  setAttribute(Qt::WA_NoSystemBackground);
  QVBoxLayout *main_layout = new QVBoxLayout(this);
  main_layout->setMargin(0);

  hlayout = new QHBoxLayout;
  hlayout->setMargin(0);
  hlayout->setSpacing(20);
  hlayout->addSpacerItem(new QSpacerItem(0, 120));

  // left icon
  if (!icon.isEmpty()) {
    QPixmap pix(icon);
    QLabel *icon = new QLabel();
    icon->setPixmap(pix.scaledToWidth(80, Qt::SmoothTransformation));
    icon->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
    hlayout->addWidget(icon);
  }

  // title
  title_label = new QPushButton(title);
  title_label->setStyleSheet("font-size: 50px; font-weight: 400; text-align: left;background-color: transparent");
  hlayout->addWidget(title_label);

  main_layout->addLayout(hlayout);

  // description
  if (!desc.isEmpty()) {
    description = new QLabel(desc);
    description->setContentsMargins(40, 20, 40, 20);
    description->setStyleSheet("font-size: 40px; color:grey");
    description->setWordWrap(true);
    description->setVisible(false);
    main_layout->addWidget(description);

    connect(title_label, &QPushButton::clicked, [=]() {
      if (!description->isVisible()) {
        emit showDescription();
      }
      description->setVisible(!description->isVisible());
    });
  }
  main_layout->addStretch();
}

void AbstractControl::hideEvent(QHideEvent *e) {
  if(description != nullptr) {
    description->hide();
  }
}

// controls

ButtonControl::ButtonControl(const QString &title, const QString &text, const QString &desc, QWidget *parent) : AbstractControl(title, desc, "", parent) {
  btn.setText(text);
  btn.setStyleSheet(R"(
    QPushButton {
      padding: 0;
      border-radius: 50px;
      font-size: 35px;
      font-weight: 500;
      color: #E4E4E4;
      background-color: #393939;
    }
    QPushButton:disabled {
      color: #33E4E4E4;
    }
  )");
  btn.setFixedSize(250, 100);
  QObject::connect(&btn, &QPushButton::released, this, &ButtonControl::released);
  hlayout->addWidget(&btn);
}

class Delegate : public QStyledItemDelegate {
  public:
  Delegate(QObject *parent = nullptr) :  QStyledItemDelegate(parent) {

  }

  void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
    qInfo() << "QStyledItemDelegate::paint";

  }
  QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override {
    // QSize  size = QStyledItemDelegate::sizeHint(option, index);
    // QWidget *w = reinterpret_cast<QWidget*>(index.data(Qt::UserRole).value<void*>());
    // QSize size = w->sizeHint();
    // qInfo() << "sizeHint****";
    return QSize(0, 200);
  }
};

ListWidget::ListWidget(QWidget *parent) : QListWidget(parent) {
  // layout_.setMargin(0);
  // default spacing is 25
  // setSpacing(25);
  setAutoFillBackground(true);
  setItemDelegate(new Delegate(this));
  setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
  // QScroller::grabGesture(this, QScroller::LeftMouseButtonGesture);
  QScroller *scroller = QScroller::scroller(this->viewport());
  QScrollerProperties sp = scroller->scrollerProperties();

  sp.setScrollMetric(QScrollerProperties::VerticalOvershootPolicy, QVariant::fromValue<QScrollerProperties::OvershootPolicy>(QScrollerProperties::OvershootAlwaysOff));
  sp.setScrollMetric(QScrollerProperties::HorizontalOvershootPolicy, QVariant::fromValue<QScrollerProperties::OvershootPolicy>(QScrollerProperties::OvershootAlwaysOff));

  scroller->grabGesture(this->viewport(), QScroller::LeftMouseButtonGesture);
  scroller->setScrollerProperties(sp);
}

void ListWidget::addItem(QWidget *w) { 
  // layout_.addWidget(w); 
  
  QListWidgetItem *item = new QListWidgetItem(this);
  // QVariant v =w;
  // item->setData(Qt::UserRole, w);
  QListWidget::addItem(item);
  setItemWidget(item, w);

}

void ListWidget::paintEvent(QPaintEvent *e) {
  // QPainter p(this);
  qInfo() << e->rect();
  // p.setPen(Qt::gray);
  // for (int i = 0; i < layout_.count() - 1; ++i) {
  //   QRect r = layout_.itemAt(i)->geometry();
  //   int bottom = r.bottom() + layout_.spacing() / 2;
  //   p.drawLine(r.left() + 40, bottom, r.right() - 40, bottom);
  // }
}