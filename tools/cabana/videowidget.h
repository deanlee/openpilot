#pragma once

#include <map>
#include <memory>

#include <QLabel>
#include <QSlider>
#include <QToolButton>

#include "selfdrive/ui/qt/widgets/cameraview.h"
#include "tools/cabana/streams/abstractstream.h"
#include "tools/cabana/streams/replaystream.h"

struct AlertInfo {
  cereal::ControlsState::AlertStatus status;
  QString text1;
  QString text2;
};

class InfoLabel : public QWidget {
public:
  InfoLabel(QWidget *parent);
  void showPixmap(const QPoint &pt, const QString &sec, const QPixmap &pm, const AlertInfo &alert);
  void showAlert(const AlertInfo &alert);
  void paintEvent(QPaintEvent *event) override;
  QPixmap pixmap;
  QString second;
  AlertInfo alert_info;
};

class Slider : public QSlider {
  Q_OBJECT

public:
  Slider(QWidget *parent);
  double currentSecond() const { return value() / factor; }
  void setCurrentSecond(double sec) { setValue(sec * factor); }
  void setTimeRange(double min, double max) { setRange(min * factor, max * factor); }

  const double factor = 1000.0;
private:
  void mousePressEvent(QMouseEvent *e) override;
  void paintEvent(QPaintEvent *ev) override;

};

class VideoWidget : public QWidget {
  Q_OBJECT

public:
  VideoWidget(QWidget *parnet = nullptr);
  void zoomChanged(double min, double max, bool is_zoomed);
  void setMaximumTime(double sec);
  AlertInfo alertInfo(double sec);
  QPixmap thumbnail(double sec);
  void showTip(double sec);

signals:
  void updateMaximumTime(double);

protected:
  bool eventFilter(QObject *obj, QEvent *event) override;
  void parseQLog(int segnum, std::shared_ptr<LogReader> qlog);
  void setTimeRange(double min, double max);
  void updateState();

  CameraWidget *cam_widget;
  Slider *slider;
  QMap<uint64_t, QPixmap> thumbnails;
  std::map<uint64_t, AlertInfo> alerts;
  InfoLabel *alert_label;
  InfoLabel thumbnail_label;
  double maximum_time = 0;
  QLabel *end_time_label;
  QLabel *time_label;
  bool zoomed = false;
};

class ControlsView : public QFrame {
  Q_OBJECT

public:
  ControlsView(QWidget *parent);
  void zoomChanged(double min, double max, bool is_zoomed);

private:
  void updatePlayBtnState();

  VideoWidget *video = nullptr;
  QToolButton *play_btn;
  QToolButton *skip_to_end_btn = nullptr;
};
