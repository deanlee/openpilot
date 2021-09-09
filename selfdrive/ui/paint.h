#pragma once

#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include "selfdrive/ui/qt/widgets/cameraview.h"
#include "selfdrive/ui/ui.h"

void ui_draw(UIState *s, int w, int h);
void ui_draw_image(const UIState *s, const Rect &r, const char *name, float alpha);
void ui_draw_rect(NVGcontext *vg, const Rect &r, NVGcolor color, int width, float radius = 0);
void ui_fill_rect(NVGcontext *vg, const Rect &r, const NVGpaint &paint, float radius = 0);
void ui_fill_rect(NVGcontext *vg, const Rect &r, const NVGcolor &color, float radius = 0);
void ui_nvg_init(UIState *s);
void ui_resize(UIState *s, int width, int height);

class IconItem : public QGraphicsItem {
public:
  IconItem(const QString &fn);
  void setColor(const QColor color, float opacity = 1.0);
  QRectF boundingRect() const override;
  void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
  const float img_size = 144, radius = 192;
  float opacity_ = 1.0;
  QColor color_;
  QPixmap img;
};

// ***** onroad widgets *****
class MaxSpeed : public QGraphicsItem {
public:
  MaxSpeed();
  void setMaxSpeed(const QString &maxSpeed) { 
    if (maxSpeed_ != maxSpeed) {
      maxSpeed_ = maxSpeed; 
      update();
    }
  }
  QRectF boundingRect() const override;
  void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
  QString maxSpeed_;
};

// container window for the NVG UI
class NvgWindow : public CameraViewWidget {
  Q_OBJECT

public:
  explicit NvgWindow(VisionStreamType type, QWidget* parent = 0) : CameraViewWidget(type, true, parent) {}
  void updateState(const UIState &s);

protected:
  void paintGL() override;
  void initializeGL() override;
  double prev_draw_t = 0;
};


class OnroadGraphicsView : public QGraphicsView {
  Q_OBJECT

public:
  OnroadGraphicsView(QWidget* parent = 0);
  void addMapToScene(QWidget *m, int width);

private:
  void resizeEvent(QResizeEvent* event) override;
  void drawBackground(QPainter *painter, const QRectF &rect) override;
  QGraphicsScene *scene_;
//   OnroadAlerts *alerts;
  MaxSpeed *maxSpeed;
  QGraphicsSimpleTextItem *speed, *unit;
  QGraphicsProxyWidget *map_proxy = nullptr;
  IconItem *wheel, *face;
  NvgWindow *nvg;
  
public slots:
  void updateState(const UIState &s);
};