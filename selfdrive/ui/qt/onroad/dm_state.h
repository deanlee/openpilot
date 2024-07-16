#pragma once

#include <QObject>
#include <QPixmap>

#include "selfdrive/ui/ui.h"

class DriverMonitorState : public QObject {
  Q_OBJECT

public:
  DriverMonitorState(QObject *parent = nullptr);
  void update(const SubMaster &sm);
  void draw(QPainter &painter, const QRect &rect);

private:
  void updateDriverState(const cereal::DriverStateV2::Reader &driverstate);

  QPixmap dm_img;
  bool dmActive = false;
  bool is_rhd = false;
  float dm_fade_state = 1.0;

  float driver_pose_vals[3];
  float driver_pose_diff[3];
  float driver_pose_sins[3];
  float driver_pose_coss[3];

  static constexpr vec3 default_face_kpts_3d[] = {
    {-5.98, -51.20, 8.00}, {-17.64, -49.14, 8.00}, {-23.81, -46.40, 8.00}, {-29.98, -40.91, 8.00}, {-32.04, -37.49, 8.00},
    {-34.10, -32.00, 8.00}, {-36.16, -21.03, 8.00}, {-36.16, 6.40, 8.00}, {-35.47, 10.51, 8.00}, {-32.73, 19.43, 8.00},
    {-29.30, 26.29, 8.00}, {-24.50, 33.83, 8.00}, {-19.01, 41.37, 8.00}, {-14.21, 46.17, 8.00}, {-12.16, 47.54, 8.00},
    {-4.61, 49.60, 8.00}, {4.99, 49.60, 8.00}, {12.53, 47.54, 8.00}, {14.59, 46.17, 8.00}, {19.39, 41.37, 8.00},
    {24.87, 33.83, 8.00}, {29.67, 26.29, 8.00}, {33.10, 19.43, 8.00}, {35.84, 10.51, 8.00}, {36.53, 6.40, 8.00},
    {36.53, -21.03, 8.00}, {34.47, -32.00, 8.00}, {32.42, -37.49, 8.00}, {30.36, -40.91, 8.00}, {24.19, -46.40, 8.00},
    {18.02, -49.14, 8.00}, {6.36, -51.20, 8.00}, {-5.98, -51.20, 8.00},
  };
  vec3 face_kpts_draw[std::size(default_face_kpts_3d)];
};
