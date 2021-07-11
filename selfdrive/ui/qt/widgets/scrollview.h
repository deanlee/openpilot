#pragma once

#include <QScrollArea>
#include <QPaintEvent>
class ScrollView : public QScrollArea {
  Q_OBJECT

public:
  explicit ScrollView(QWidget *w = nullptr, QWidget *parent = nullptr);
protected:
  void hideEvent(QHideEvent *e) override;
  void paintEvent(QPaintEvent *event) override;
  bool viewportEvent(QEvent *event) override;
  void scrollContentsBy(int dx, int dy) override;
};
