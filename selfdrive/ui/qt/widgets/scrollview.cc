#include "selfdrive/ui/qt/widgets/scrollview.h"

#include <QScrollBar>
#include <QScroller>

ScrollView::ScrollView(QWidget *w, QWidget *parent) : QScrollArea(parent) {
  setAutoFillBackground(true);
  setBackgroundRole(QPalette::Dark);
  setWidget(w);
  setWidgetResizable(true);

  // setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  // setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  // setStyleSheet("ScrollView { background-color:transparent; }");

  QString style = R"(
    QScrollBar:vertical {
      border: none;
      background: transparent;
      width:50px;
      margin: 0;
    }
    QScrollBar::handle:vertical {
      min-height: 0px;
      border-radius: 4px;
      background-color: white;
    }
    QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
      height: 0px;
    }
    QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
      background: none;
    }
  )";

  verticalScrollBar()->setStyleSheet(style);
  horizontalScrollBar()->setStyleSheet(style);

  // QScroller *scroller = QScroller::scroller(this->viewport());
  // QScrollerProperties sp = scroller->scrollerProperties();

  // sp.setScrollMetric(QScrollerProperties::VerticalOvershootPolicy, QVariant::fromValue<QScrollerProperties::OvershootPolicy>(QScrollerProperties::OvershootAlwaysOff));
  // sp.setScrollMetric(QScrollerProperties::HorizontalOvershootPolicy, QVariant::fromValue<QScrollerProperties::OvershootPolicy>(QScrollerProperties::OvershootAlwaysOff));

  // scroller->grabGesture(this->viewport(), QScroller::TouchGesture);
  // scroller->setScrollerProperties(sp);
}

void ScrollView::hideEvent(QHideEvent *e) {
  verticalScrollBar()->setValue(0);
}

void ScrollView::paintEvent(QPaintEvent *event) {
  printf("ScrollView::paintEvent %d %d\n", event->rect().width(), event->rect().height());
}

bool ScrollView::viewportEvent(QEvent *event) {
  return QScrollArea::viewportEvent(event);
}

void ScrollView::scrollContentsBy(int dx, int dy) {
  // widget()->scroll(dx, dy);
  return QScrollArea::scrollContentsBy(dx, dy);
}