#include "selfdrive/ui/qt/onroad/driver_monitor.h"
#include "common/mat.h"
#include <cmath>
#include <raylib.h>
#include <iterator>
#include "selfdrive/ui/qt/onroad/raylib_helpers.h"

const int btn_size = 192;
const int img_size = (btn_size / 4) * 3;


constexpr vec3 default_face_kpts_3d[] = {
  {-5.98, -51.20, 8.00}, {-17.64, -49.14, 8.00}, {-23.81, -46.40, 8.00}, {-29.98, -40.91, 8.00}, {-32.04, -37.49, 8.00},
  {-34.10, -32.00, 8.00}, {-36.16, -21.03, 8.00}, {-36.16, 6.40, 8.00}, {-35.47, 10.51, 8.00}, {-32.73, 19.43, 8.00},
  {-29.30, 26.29, 8.00}, {-24.50, 33.83, 8.00}, {-19.01, 41.37, 8.00}, {-14.21, 46.17, 8.00}, {-12.16, 47.54, 8.00},
  {-4.61, 49.60, 8.00}, {4.99, 49.60, 8.00}, {12.53, 47.54, 8.00}, {14.59, 46.17, 8.00}, {19.39, 41.37, 8.00},
  {24.87, 33.83, 8.00}, {29.67, 26.29, 8.00}, {33.10, 19.43, 8.00}, {35.84, 10.51, 8.00}, {36.53, 6.40, 8.00},
  {36.53, -21.03, 8.00}, {34.47, -32.00, 8.00}, {32.42, -37.49, 8.00}, {30.36, -40.91, 8.00}, {24.19, -46.40, 8.00},
  {18.02, -49.14, 8.00}, {6.36, -51.20, 8.00}, {-5.98, -51.20, 8.00},
};

vec3 face_kpts_draw[std::size(default_face_kpts_3d)];
Texture2D dm_texture;

DriverMonitor::DriverMonitor() {
  dm_texture = LoadTextureResized("../assets/img_driver_face.png", img_size + 5);
}

DriverMonitor::~DriverMonitor() {
  UnloadTexture(dm_texture);
}

void DriverMonitor::updateState(const UIState &s) {
  const auto &driverstate = (*(s.sm))["driverStateV2"].getDriverStateV2();
  auto dm_state = (*(s.sm))["driverMonitoringState"].getDriverMonitoringState();
  dmActive = dm_state.getIsActiveMode();
  is_rhd = dm_state.getIsRHD();
  // DM icon transition
  dm_fade_state = std::clamp(dm_fade_state + 0.2 * (0.5 - dmActive), 0.0, 1.0);

  const auto driver_orient = is_rhd ? driverstate.getRightDriverData().getFaceOrientation() : driverstate.getLeftDriverData().getFaceOrientation();
  for (int i = 0; i < std::size(driver_pose_vals); i++) {
    float v_this = (i == 0 ? (driver_orient[i] < 0 ? 0.7 : 0.9) : 0.4) * driver_orient[i];
    driver_pose_diff[i] = fabs(driver_pose_vals[i] - v_this);
    driver_pose_vals[i] = 0.8 * v_this + (1 - 0.8) * driver_pose_vals[i];
    driver_pose_sins[i] = sinf(driver_pose_vals[i] * (1.0 - dm_fade_state));
    driver_pose_coss[i] = cosf(driver_pose_vals[i] * (1.0 - dm_fade_state));
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
    face_kpts_draw[kpi] = (vec3){{kpt_this.v[0], kpt_this.v[1], (float)(kpt_this.v[2] * (1.0 - dm_fade_state) + 8 * dm_fade_state)}};
  }
}

void DriverMonitor::draw() {
  // base icon
  int offset = UI_BORDER_SIZE + btn_size / 2;
  int x = is_rhd ? GetScreenWidth() - offset : offset;
  int y = GetScreenHeight() - offset;
  uint8_t opacity = dmActive ? 164 : 51;
  // drawCircleTexture(dm_texture, x, y, blackColor(70), opacity);
  drawCircleTexture(&dm_texture, x, y, opacity);

  // // face
  std::vector<Vector2> points(std::size(default_face_kpts_3d));
  for (int i = 0; i < points.size(); ++i) {
    const auto &v = face_kpts_draw[i].v;
    float kp = (v[2] - 8) / 120 + 1.0;
    points[i] = (Vector2{v[0] * kp + x, v[1] * kp + y});
  }

  // painter.setPen(QPen(QColor::fromRgbF(1.0, 1.0, 1.0, opacity), 5.2, Qt::SolidLine, Qt::RoundCap));
  // painter.drawPolyline(face_kpts_draw, std::size(default_face_kpts_3d));
  DrawSplineLinear(points.data(), points.size(), 5.2, {255, 255, 255, opacity});

  // // tracking arcs
  const int arc_l = 133;
  // const float arc_t_default = 6.7;
  // const float arc_t_extend = 12.0;
  auto *s = uiState();
  Color arc_color = {(uint8_t)(0.545 - 0.445 * s->engaged() * 255),
                     (uint8_t)(0.545 + 0.4 * s->engaged() * 255),
                     (uint8_t)(0.545 - 0.285 * s->engaged() * 255),
                     (uint8_t)(0.4 * (1.0 - dm_fade_state) * 255)};
  float delta_x = -driver_pose_sins[1] * arc_l / 2;
  // float delta_y = -driver_pose_sins[0] * arc_l / 2;
  // QRectF(std::fmin(x + delta_x, x), y - arc_l / 2, fabs(delta_x), arc_l)
  DrawCircleSectorLines(Vector2{(float)(x + delta_x), (float)(y - arc_l)}, 1, (driver_pose_sins[1] > 0 ? 90 : -90) * 16, 180 * 16, 10, arc_color);
  // DrawArc()
  // painter.setPen(QPen(arc_color, arc_t_default+arc_t_extend*fmin(1.0, scene.driver_pose_diff[1] * 5.0), Qt::SolidLine, Qt::RoundCap));
  // painter.drawArc(QRectF(std::fmin(x + delta_x, x), y - arc_l / 2, fabs(delta_x), arc_l), (scene.driver_pose_sins[1]>0 ? 90 : -90) * 16, 180 * 16);
  // painter.setPen(QPen(arc_color, arc_t_default+arc_t_extend*fmin(1.0, scene.driver_pose_diff[0] * 5.0), Qt::SolidLine, Qt::RoundCap));
  // painter.drawArc(QRectF(x - arc_l / 2, std::fmin(y + delta_y, y), arc_l, fabs(delta_y)), (scene.driver_pose_sins[0]>0 ? 0 : 180) * 16, 180 * 16);
}
