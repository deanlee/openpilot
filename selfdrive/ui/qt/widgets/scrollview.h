#pragma once

#include <QScrollArea>
#include <QGraphicsView>
#include <QResizeEvent>
#include <QGraphicsProxyWidget>
#include <QGraphicsScene>
#include <QOpenGLWidget>
#include <QResizeEvent>
#include <QPixmapCache>
class ScrollView : public QGraphicsView {
  Q_OBJECT

public:
  explicit ScrollView(QWidget *w = nullptr, QWidget *parent = nullptr);
protected:
  void hideEvent(QHideEvent *e) override;
  void resizeEvent(QResizeEvent *event) override;
  QGraphicsProxyWidget *proxyWidget;
  QWidget *w_;
};
