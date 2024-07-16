#include "selfdrive/ui/qt/onroad/dm_state.h"

#include <cmath>

#include "selfdrive/ui/qt/util.h"
#include "selfdrive/ui/qt/onroad/buttons.h"

QColor blackColor(int alpha = 255) { return QColor(0, 0, 0, alpha); }

DriverMonitorState::DriverMonitorState(QObject *parent) : QObject(parent) {
  dm_img = loadPixmap("../assets/img_driver_face.png", {img_size + 5, img_size + 5});
}

void DriverMonitorState::update(const SubMaster &sm) {
  auto dm_state = sm["driverMonitoringState"].getDriverMonitoringState();
  dmActive = dm_state.getIsActiveMode();
  is_rhd = dm_state.getIsRHD();
  dm_fade_state = std::clamp(dm_fade_state + 0.2 * (0.5 - dmActive), 0.0, 1.0);

  auto s = uiState();
  if (sm.rcv_frame("driverStateV2") > s->scene.started_frame) {
    updateDriverState(sm["driverStateV2"].getDriverStateV2());
  }
}

void DriverMonitorState::updateDriverState(const cereal::DriverStateV2::Reader &driverstate) {
  auto state = is_rhd ? driverstate.getRightDriverData() : driverstate.getLeftDriverData();
  auto driver_orient = state.getFaceOrientation();
  for (int i = 0; i < std::size(driver_pose_vals); i++) {
    float v_this = (i == 0 ? (driver_orient[i] < 0 ? 0.7 : 0.9) : 0.4) * driver_orient[i];
    driver_pose_diff[i] = fabs(driver_pose_vals[i] - v_this);
    driver_pose_vals[i] = 0.8 * v_this + (1 - 0.8) * driver_pose_vals[i];
    driver_pose_sins[i] = sinf(driver_pose_vals[i]*(1.0-dm_fade_state));
    driver_pose_coss[i] = cosf(driver_pose_vals[i]*(1.0-dm_fade_state));
  }

  auto [sin_y, sin_x, sin_z] = driver_pose_sins;
  auto [cos_y, cos_x, cos_z] = driver_pose_coss;

  const mat3 r_xyz = (mat3){{
    cos_x * cos_z,
    cos_x * sin_z,
    -sin_x,

    -sin_y * sin_x * cos_z - cos_y * sin_z,
    -sin_y * sin_x * sin_z + cos_y * cos_z,
    -sin_y * cos_x,

    cos_y * sin_x * cos_z - sin_y * sin_z,
    cos_y * sin_x * sin_z + sin_y * cos_z,
    cos_y * cos_x,
  }};

  // transform vertices
  for (int kpi = 0; kpi < std::size(default_face_kpts_3d); kpi++) {
    vec3 kpt_this = matvecmul3(r_xyz, default_face_kpts_3d[kpi]);
    face_kpts_draw[kpi] = (vec3){{kpt_this.v[0], kpt_this.v[1], (float)(kpt_this.v[2] * (1.0-dm_fade_state) + 8 * dm_fade_state)}};
  }
}

void DriverMonitorState::draw(QPainter &painter, const QRect &rect) {
  painter.save();

  // base icon
  int offset = UI_BORDER_SIZE + btn_size / 2;
  int x = is_rhd ? rect.width() - offset : offset;
  int y = rect.height() - offset;
  float opacity = dmActive ? 0.65 : 0.2;
  drawIcon(painter, QPoint(x, y), dm_img, blackColor(70), opacity);

  // face
  QPointF points[std::size(default_face_kpts_3d)];
  for (int i = 0; i < std::size(default_face_kpts_3d); ++i) {
    float kp = (face_kpts_draw[i].v[2] - 8) / 120 + 1.0;
    points[i] = QPointF(face_kpts_draw[i].v[0] * kp + x, face_kpts_draw[i].v[1] * kp + y);
  }

  painter.setPen(QPen(QColor::fromRgbF(1.0, 1.0, 1.0, opacity), 5.2, Qt::SolidLine, Qt::RoundCap));
  painter.drawPolyline(points, std::size(default_face_kpts_3d));

  // tracking arcs
  const int arc_l = 133;
  const float arc_t_default = 6.7;
  const float arc_t_extend = 12.0;
  auto s = uiState();
  QColor arc_color = QColor::fromRgbF(0.545 - 0.445 * s->engaged(),
                                      0.545 + 0.4 * s->engaged(),
                                      0.545 - 0.285 * s->engaged(),
                                      0.4 * (1.0 - dm_fade_state));
  float delta_x = -driver_pose_sins[1] * arc_l / 2;
  float delta_y = -driver_pose_sins[0] * arc_l / 2;
  painter.setPen(QPen(arc_color, arc_t_default+arc_t_extend*fmin(1.0, driver_pose_diff[1] * 5.0), Qt::SolidLine, Qt::RoundCap));
  painter.drawArc(QRectF(std::fmin(x + delta_x, x), y - arc_l / 2, fabs(delta_x), arc_l), (driver_pose_sins[1]>0 ? 90 : -90) * 16, 180 * 16);
  painter.setPen(QPen(arc_color, arc_t_default+arc_t_extend*fmin(1.0, driver_pose_diff[0] * 5.0), Qt::SolidLine, Qt::RoundCap));
  painter.drawArc(QRectF(x - arc_l / 2, std::fmin(y + delta_y, y), arc_l, fabs(delta_y)), (driver_pose_sins[0]>0 ? 0 : 180) * 16, 180 * 16);

  painter.restore();
}
