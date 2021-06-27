#pragma once

#include <memory>

#include <QStackedLayout>
#include <QGraphicsView>

#include "selfdrive/common/util.h"
#include "selfdrive/ui/qt/widgets/cameraview.h"

class DriverViewScene : public QWidget {
  Q_OBJECT

public:
  explicit DriverViewScene(QWidget *parent);

public slots:
  void frameUpdated();

protected:
  void showEvent(QShowEvent *event) override;
  void hideEvent(QHideEvent *event) override;
  void paintEvent(QPaintEvent *event) override;

private:
  Params params;
  SubMaster sm;
  QImage face;
  bool is_rhd = false;
  bool frame_updated = false;
};

class DriverViewWindow : public QGraphicsView {
  Q_OBJECT

public:
  explicit DriverViewWindow(QWidget *parent);

signals:
  void done();

protected:
  void showEvent(QShowEvent *event) override;
  void hideEvent(QHideEvent *event) override;
  void mousePressEvent(QMouseEvent* e) override;
  void resizeEvent(QResizeEvent *event) override;

private:
  CameraViewWidget *cameraView;
  Params params;
  SubMaster sm;
  QImage face;
  bool is_rhd = false;
  QGraphicsRectItem *rect_;
  bool frame_updated = false;
  // DriverViewScene *scene;
  // QStackedLayout *layout;
};
