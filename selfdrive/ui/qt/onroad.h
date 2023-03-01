#pragma once

#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsView>

#include <QPushButton>
#include <QStackedLayout>
#include <QWidget>

#include "common/util.h"
#include "selfdrive/ui/ui.h"
#include "selfdrive/ui/qt/widgets/cameraview.h"


const int btn_size = 192;
const int img_size = (btn_size / 4) * 3;

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

class IconItem : public QGraphicsItem {
public:
  IconItem(QPixmap pm, QGraphicsItem *parent = 0);
  QRectF boundingRect() const override { return {0, 0, (qreal)radius, (qreal)radius}; }
  void update(const QColor color, float opacity);

  const int radius = 192;
  const int img_size = (radius / 2) * 1.5;

protected:
  void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr) override;
  QPixmap pixmap;
  QColor bg;
  bool opacity = 1.0;
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

class ExperimentalButton : public QPushButton {
  Q_OBJECT

public:
  explicit ExperimentalButton(QWidget *parent = 0);
  void updateState(const UIState &s);

private:
  void paintEvent(QPaintEvent *event) override;

  Params params;
  QPixmap engage_img;
  QPixmap experimental_img;
};


class OnroadScene : public QGraphicsScene {
public:
  explicit OnroadScene(QObject *parent);
  void updateState(const UIState &s);
  void setGeometry(const QRectF &rect);

private:
  QGraphicsRectItem *header;
  OnroadAlerts *alerts;
  MaxSpeedItem *max_speed;
  CurrentSpeedItem *current_speed;
  IconItem *dm, *wheel;
  // float dm_fade_state = 0;
};


class OnroadView : public QGraphicsView {
  Q_OBJECT
public:
  OnroadView(VisionStreamType type, QWidget *parent);
  bool isMapVisible() const { return false;}
  void drawBackground(QPainter *painter, const QRectF &rect) override;
  void updateFrameMat();

// public slots:
//   void updateState(const UIState &s);

private:
  void resizeEvent(QResizeEvent *event) override;
  void drawLaneLines(QPainter &painter, const UIState *s);
  void drawLead(QPainter &painter, const cereal::RadarState::LeadData::Reader &lead_data, const QPointF &vd);
  inline QColor redColor(int alpha = 255) { return QColor(201, 34, 49, alpha); }
  inline QColor whiteColor(int alpha = 255) { return QColor(255, 255, 255, alpha); }
  inline QColor blackColor(int alpha = 255) { return QColor(0, 0, 0, alpha); }

  CameraWidget * cam_widget;
  OnroadScene *onroad_scene;
};


// container window for the NVG UI
class AnnotatedCameraWidget : public CameraWidget {
  Q_OBJECT
  Q_PROPERTY(float speed MEMBER speed);
  Q_PROPERTY(QString speedUnit MEMBER speedUnit);
  Q_PROPERTY(float setSpeed MEMBER setSpeed);
  Q_PROPERTY(float speedLimit MEMBER speedLimit);
  Q_PROPERTY(bool is_cruise_set MEMBER is_cruise_set);
  Q_PROPERTY(bool has_eu_speed_limit MEMBER has_eu_speed_limit);
  Q_PROPERTY(bool has_us_speed_limit MEMBER has_us_speed_limit);
  Q_PROPERTY(bool is_metric MEMBER is_metric);

  Q_PROPERTY(bool dmActive MEMBER dmActive);
  Q_PROPERTY(bool hideDM MEMBER hideDM);
  Q_PROPERTY(bool rightHandDM MEMBER rightHandDM);
  Q_PROPERTY(int status MEMBER status);

public:
  explicit AnnotatedCameraWidget(VisionStreamType type, QWidget* parent = 0);
  void updateState(const UIState &s);

private:
  void drawIcon(QPainter &p, int x, int y, QPixmap &img, QBrush bg, float opacity);
  void drawText(QPainter &p, int x, int y, const QString &text, int alpha = 255);

  ExperimentalButton *experimental_btn;
  QPixmap dm_img;
  float speed;
  QString speedUnit;
  float setSpeed;
  float speedLimit;
  bool is_cruise_set = false;
  bool is_metric = false;
  bool dmActive = false;
  bool hideDM = false;
  bool rightHandDM = false;
  float dm_fade_state = 1.0;
  bool has_us_speed_limit = false;
  bool has_eu_speed_limit = false;
  bool v_ego_cluster_seen = false;
  int status = STATUS_DISENGAGED;
  std::unique_ptr<PubMaster> pm;

  int skip_frame_count = 0;
  bool wide_cam_requested = false;

protected:
  void paintGL() override;
  void initializeGL() override;
  void showEvent(QShowEvent *event) override;
  void updateFrameMat() override;
  // void drawLaneLines(QPainter &painter, const UIState *s);
  // void drawLead(QPainter &painter, const cereal::RadarState::LeadData::Reader &lead_data, const QPointF &vd);
  void drawHud(QPainter &p);
  void drawDriverState(QPainter &painter, const UIState *s);
  inline QColor redColor(int alpha = 255) { return QColor(201, 34, 49, alpha); }
  inline QColor whiteColor(int alpha = 255) { return QColor(255, 255, 255, alpha); }
  inline QColor blackColor(int alpha = 255) { return QColor(0, 0, 0, alpha); }

  double prev_draw_t = 0;
  FirstOrderFilter fps_filter;
};

// container for all onroad widgets
class OnroadWindow : public QWidget {
  Q_OBJECT

public:
  OnroadWindow(QWidget* parent = 0);
  bool isMapVisible() const { return map && map->isVisible(); }

private:
  void paintEvent(QPaintEvent *event);
  void mousePressEvent(QMouseEvent* e) override;
  OnroadView *nvg;
  QColor bg = bg_colors[STATUS_DISENGAGED];
  QWidget *map = nullptr;
  QHBoxLayout* split;

private slots:
  void offroadTransition(bool offroad);
  void updateState(const UIState &s);
};
