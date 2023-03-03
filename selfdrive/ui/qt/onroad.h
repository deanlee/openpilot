#pragma once

#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QPushButton>
#include <QStackedLayout>
#include <QWidget>

#include "common/util.h"
#include "selfdrive/ui/qt/widgets/cameraview.h"
#include "selfdrive/ui/ui.h"

const int btn_size = 192;
const int img_size = (btn_size / 4) * 3;
inline QColor redColor(int alpha = 255) { return QColor(201, 34, 49, alpha); }
inline QColor whiteColor(int alpha = 255) { return QColor(255, 255, 255, alpha); }
inline QColor blackColor(int alpha = 255) { return QColor(0, 0, 0, alpha); }

class MaxSpeedItem : public QGraphicsItem {
public:
  MaxSpeedItem(QGraphicsItem *parent = nullptr) : QGraphicsItem(parent) {}
  void updateState(const UIState &s);
  QRectF boundingRect() const override { return {0, 0, 184 + 10, 202 + 10}; }

protected:
  void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = 0) override;
  QString maxSpeed;
  bool is_cruise_set = false;
};

class CurrentSpeedItem : public QGraphicsItem {
public:
  CurrentSpeedItem(QGraphicsItem *parent = nullptr) : QGraphicsItem(parent) {}
  void updateState(const UIState &s);
  QRectF boundingRect() const override { return {0, 0, 300, 300}; }

protected:
  void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = 0) override;
  QString speed;
  QString speedUnit;
  bool v_ego_cluster_seen = false;
};


class SpeedLimitItem : public QGraphicsItem {
public:
  SpeedLimitItem(QGraphicsItem *parent = nullptr) : QGraphicsItem(parent) {}
  void updateState(const UIState &s);
  QRectF boundingRect() const override;

protected:
  void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = 0) override;
  QString speedLimitStr;
  QString speedUnit;
  bool has_us_speed_limit = false;
  bool has_eu_speed_limit = false;
  // bool v_ego_cluster_seen = false;
};

class DriverStateItem : public QGraphicsItem {
public:
  DriverStateItem(QGraphicsItem *parent = 0) : QGraphicsItem(parent) {}
  QRectF boundingRect() const override { return {0, 0, 300, 300}; }
  void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr) override;
  void updateState(const UIState &s);
  void drawIcon(QPainter &p, int x, int y, QPixmap &img, QBrush bg, float opacity);
  float dm_fade_state = 0;
  bool dmActive = false;
  bool rightHandDM = false;
  QPixmap dm_img;
};

// ***** onroad widgets *****
class OnroadAlerts : public QGraphicsRectItem {
public:
  OnroadAlerts(QGraphicsItem *parent = 0) : QGraphicsRectItem(parent) {}
  void update(const Alert &a, const QColor &color);

protected:
  void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr) override;
  QColor bg;
  Alert alert = {};
};

class ExperimentalButton : public QGraphicsItem {
public:
  explicit ExperimentalButton(QGraphicsItem *parent = 0);
  void updateState(const UIState &s);
  QRectF boundingRect() const override { return {0, 0, btn_size, btn_size}; }

private:
  void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr) override;

  Params params;
  QPixmap engage_img;
  QPixmap experimental_img;
};

class OnroadScene : public QGraphicsScene {
public:
  explicit OnroadScene(QObject *parent);
  void updateState();
  void setGeometry(const QRectF &rect);

private:
  QGraphicsRectItem *header;
  OnroadAlerts *alerts;
  MaxSpeedItem *max_speed;
  CurrentSpeedItem *current_speed;
  DriverStateItem *driver_state;
  ExperimentalButton *experimental_btn;
  SpeedLimitItem *speed_limit;
};

class OnroadView : public QGraphicsView {
  Q_OBJECT
public:
  OnroadView(VisionStreamType type, QWidget *parent);
  bool isMapVisible() const { return false; }
  void drawBackground(QPainter *painter, const QRectF &rect) override;
  void updateFrameMat();

private:
  void resizeEvent(QResizeEvent *event) override;
  void drawLaneLines(QPainter &painter, const UIState *s);
  void drawLead(QPainter &painter, const cereal::RadarState::LeadData::Reader &lead_data, const QPointF &vd);

  CameraWidget *cam_widget;
  OnroadScene *onroad_scene;
};

// container for all onroad widgets
class OnroadWindow : public QWidget {
  Q_OBJECT

public:
  OnroadWindow(QWidget *parent = 0);
  bool isMapVisible() const { return map && map->isVisible(); }

private:
  void paintEvent(QPaintEvent *event);
  void mousePressEvent(QMouseEvent *e) override;
  OnroadView *nvg;
  QColor bg = bg_colors[STATUS_DISENGAGED];
  QWidget *map = nullptr;
  QHBoxLayout *split;

private slots:
  void offroadTransition(bool offroad);
  void updateState(const UIState &s);
};
