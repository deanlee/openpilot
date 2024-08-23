
#include "selfdrive/ui/qt/onroad/annotated_camera.h"

#include <raylib.h>
#include <rlgl.h>
#include "selfdrive/ui/qt/onroad/raylib_helpers.h"

#include <algorithm>
#include <cmath>

#include "common/swaglog.h"
#include "common/util.h"

Texture2D dm_texture;

void DrawArc(Vector2 center, float radius, float startAngle, float endAngle, float segments, Color color) {
    if (startAngle > endAngle) {
        float temp = startAngle;
        startAngle = endAngle;
        endAngle = temp;
    }

    float angleStep = (endAngle - startAngle) / segments;

    for (float angle = startAngle; angle < endAngle; angle += angleStep) {
        float x1 = center.x + cosf(angle * DEG2RAD) * radius;
        float y1 = center.y + sinf(angle * DEG2RAD) * radius;

        float x2 = center.x + cosf((angle + angleStep) * DEG2RAD) * radius;
        float y2 = center.y + sinf((angle + angleStep) * DEG2RAD) * radius;

        DrawLine((int)x1, (int)y1, (int)x2, (int)y2, color);
    }
}


Texture2D LoadTextureResized(const char *fileName, int size) {
  Image img = LoadImage(fileName);
  ImageResize(&img, size, size);
  Texture2D texture = LoadTextureFromImage(img);
  UnloadImage(img);
  return texture;
}

void drawCircleTexture(Texture2D *texture, int x, int y, uint8_t opacity) {
  Vector2 center = {(float)(x + texture->width / 2.0), (float)(y + texture->height / 2.0)};
  DrawCircleV(center, (float)(texture->width / 2.0), {0, 0, 0, 166});
  DrawTexture(*texture, x, y, {255, 255, 255, opacity});
}

// HudView
const int btn_size = 192;
const int img_size = (btn_size / 4) * 3;

AnnotatedCamera::AnnotatedCamera() {
  camera_view = std::make_unique<CameraView>("camerad", VISION_STREAM_ROAD, true);
  hud = std::make_unique<HudView>();
}

void AnnotatedCamera::updateState(const UIState &s) {
  hud->updateState(s);
}

void AnnotatedCamera::draw() {
  camera_view->draw();
  hud->draw();
}

// HudView

HudView::HudView() {
  auto texture = LoadTextureResized("../assets/img_chffr_wheel.png", img_size);
  dm_texture = LoadTextureResized("../assets/img_driver_face.png", img_size + 5);
  wheel_texture = new Texture2D;
  *((Texture2D *)wheel_texture) = texture;
}

HudView::~HudView() {
  UnloadTexture(*((Texture2D *)wheel_texture));
  // delete wheel_texture;
  UnloadTexture(dm_texture);
}

void HudView::updateState(const UIState &s) {
  const int SET_SPEED_NA = 255;
  const SubMaster &sm = *(s.sm);

  const bool cs_alive = sm.alive("controlsState");
  const auto cs = sm["controlsState"].getControlsState();
  const auto car_state = sm["carState"].getCarState();

  is_metric = s.scene.is_metric;

  // Handle older routes where vCruiseCluster is not set
  float v_cruise = cs.getVCruiseCluster() == 0.0 ? cs.getVCruise() : cs.getVCruiseCluster();
  setSpeed = cs_alive ? v_cruise : SET_SPEED_NA;
  is_cruise_set = setSpeed > 0 && (int)setSpeed != SET_SPEED_NA;
  if (is_cruise_set && !is_metric) {
    setSpeed *= KM_TO_MILE;
  }

  // Handle older routes where vEgoCluster is not set
  v_ego_cluster_seen = v_ego_cluster_seen || car_state.getVEgoCluster() != 0.0;
  float v_ego = v_ego_cluster_seen ? car_state.getVEgoCluster() : car_state.getVEgo();
  speed = cs_alive ? std::max<float>(0.0, v_ego) : 0.0;
  speed *= is_metric ? MS_TO_KPH : MS_TO_MPH;

  speedUnit = is_metric ? "km/h" : "mph";
  hideBottomIcons = (cs.getAlertSize() != cereal::ControlsState::AlertSize::NONE);
  status = s.status;

  // update engageability/experimental mode button
  // experimental_btn->updateState(s);

  // update DM icon
  auto dm_state = sm["driverMonitoringState"].getDriverMonitoringState();
  dmActive = dm_state.getIsActiveMode();
  rightHandDM = dm_state.getIsRHD();
  // DM icon transition
  dm_fade_state = std::clamp(dm_fade_state + 0.2 * (0.5 - dmActive), 0.0, 1.0);

  alerts.updateState(s);

  if (!hideBottomIcons && (sm.rcv_frame("driverStateV2") > s.scene.started_frame)) {
    update_dmonitoring((UIState*)(&s), sm["driverStateV2"].getDriverStateV2(), dm_fade_state, rightHandDM);
  }
}

void HudView::draw() {
  auto *s = uiState();
  auto &sm = *(s->sm);
  updateState(*s);
  s->fb_w = GetScreenWidth();
  s->fb_h = GetScreenHeight();
  s->car_space_transform.reset();
  s->car_space_transform.translate(GetScreenWidth() / 2, GetScreenHeight() / 2)
      .scale(1.1, 1.1)
      .translate(-FCAM_INTRINSIC_MATRIX.v[2], -FCAM_INTRINSIC_MATRIX.v[5]);

  // qWarning() << "before draw";
  // auto speed1 = util::string_format("%d", setSpeed);
  // DrawText(speed1.c_str(), 10, 10, 20, RED);  // Example draw call

  drawHud();

  auto model = sm["modelV2"].getModelV2();
  if (s->scene.world_objects_visible) {
    update_model(s, model);
    // drawLaneLines(painter, s);

    if (s->scene.longitudinal_control && sm.rcv_frame("radarState") > s->scene.started_frame) {
      auto radar_state = sm["radarState"].getRadarState();
      update_leads(s, radar_state, model.getPosition());
      auto lead_one = radar_state.getLeadOne();
      auto lead_two = radar_state.getLeadTwo();
      if (lead_one.getStatus()) {
        drawLead(lead_one, s->scene.lead_vertices[0]);
      }
      if (lead_two.getStatus() && (std::abs(lead_one.getDRel() - lead_two.getDRel()) > 3.0)) {
        drawLead(lead_two, s->scene.lead_vertices[1]);
      }
    }
  }

  const auto &scene = s->scene;

  // lanelines
  for (int i = 0; i < std::size(scene.lane_line_vertices); ++i) {
    std::vector<Vector2> points;
    for (auto p : scene.lane_line_vertices[i]) {
      points.push_back(Vector2{(float)p.x(), (float)p.y()});
    }
    // auto p = scene.lane_line_vertices[i].front();

    Color color = {255, 255, 255, (unsigned char)(std::clamp<float>(scene.lane_line_probs[i], 0.0, 0.7) * 255)};
    // DrawSplineBezierCubic(points.data(), points.size(), 2, color);

    DrawTriangleFan(points.data(), points.size(), color);

    //   painter.setBrush(QColor::fromRgbF(1.0, 1.0, 1.0, std::clamp<float>(scene.lane_line_probs[i], 0.0, 0.7)));
    //   painter.drawPolygon(scene.lane_line_vertices[i]);

  }
  if (!hideBottomIcons && (sm.rcv_frame("driverStateV2") > s->scene.started_frame)) {
      drawDriverState(s);
    }
  // std::vector<Vector2> points;
  // for (int i = 0; i < 50; ++i) {
  //   points.push_back(Vector2{(float)10.0 * i, (float)10.0 * i});
  // }
  // DrawSplineLinear(points.data(), points.size(), 10, YELLOW);

  // // road edges
  // for (int i = 0; i < std::size(scene.road_edge_vertices); ++i) {
  //   painter.setBrush(QColor::fromRgbF(1.0, 0, 0, std::clamp<float>(1.0 - scene.road_edge_stds[i], 0.0, 1.0)));
  //   painter. (scene.road_edge_vertices[i]);
  // }
}

void HudView::drawHud() {
  // Header gradient
  DrawRectangleGradientV(0, 0, GetScreenWidth(), UI_HEADER_HEIGHT, {0, 0, 0, 115}, {0, 0, 0, 0});
  drawCircleTexture((Texture2D *)wheel_texture, GetScreenWidth() - UI_BORDER_SIZE - ((Texture2D *)wheel_texture)->width, UI_BORDER_SIZE, 255);

  std::string speedStr = std::to_string((int)std::nearbyint(speed));
  std::string setSpeedStr = is_cruise_set ? std::to_string((int)std::nearbyint(setSpeed)) : "–";

  // // Draw outer box + border to contain set speed
  const Vector2 default_size = {172, 204};
  Vector2 set_speed_size = default_size;
  if (is_metric) set_speed_size.x = 200;

  Rectangle set_speed_rect = {60 + (default_size.x - set_speed_size.x) / 2, 45, set_speed_size.x, set_speed_size.y};
  DrawRectangleRoundedLinesEx(set_speed_rect, 0.2, 1, 6, {255, 255, 255, 75});

  // // Draw MAX
  Color max_color = {0x80, 0xd8, 0xa6, 0xff};
  Color set_speed_color = WHITE;
  if (is_cruise_set) {
    if (status == STATUS_DISENGAGED) {
      max_color = WHITE;
    } else if (status == STATUS_OVERRIDE) {
      max_color = {0x91, 0x9b, 0x95, 0xff};
    }
  } else {
    max_color = {0xa6, 0xa6, 0xa6, 0xff};
    set_speed_color = {0x72, 0x72, 0x72, 0xff};
  }
  draw_text("MAX", set_speed_rect.x, set_speed_rect.y + 27, 40, max_color);
  draw_text(setSpeedStr.c_str(), set_speed_rect.x, set_speed_rect.y + 77, 90, max_color);
  // current speed
  int textWidth = MeasureText(speedStr.c_str(), 176);
  draw_text(speedStr.c_str(), (GetScreenWidth() - textWidth) / 2, 210, 176, {255, 255, 255, 255});
  textWidth = MeasureText(speedUnit.c_str(), 66);
  draw_text(speedUnit.c_str(), (GetScreenWidth() - textWidth) / 2, 290, 66, {255, 255, 255, 200});

  alerts.draw();
}

void HudView::drawLead(const cereal::RadarState::LeadData::Reader &lead_data, const QPointF &vd) {
  const float speedBuff = 10.;
  const float leadBuff = 40.;
  const float d_rel = lead_data.getDRel();
  const float v_rel = lead_data.getVRel();

  uint8_t fillAlpha = 0;
  if (d_rel < leadBuff) {
    fillAlpha = 255 * (1.0 - (d_rel / leadBuff));
    if (v_rel < 0) {
      fillAlpha += 255 * (-1 * (v_rel / speedBuff));
    }
    fillAlpha = (int)(fmin(fillAlpha, 255));
  }

  float sz = std::clamp((25 * 30) / (d_rel / 3 + 30), 15.0f, 30.0f) * 2.35;
  float x = std::clamp((float)vd.x(), 0.f, GetScreenWidth() - sz / 2);
  float y = std::fmin(GetScreenHeight() - sz * .6, (float)vd.y());

  float g_xo = sz / 5;
  float g_yo = sz / 10;

  Vector2 glow[] = {Vector2{float(x + (sz * 1.35) + g_xo), float(y + sz + g_yo)}, Vector2{x, y - g_yo}, Vector2{x - float(sz * 1.35) - g_xo, y + sz + g_yo}};
  DrawTriangle(glow[0], glow[1], glow[2], {218, 202, 37, 255});

  // chevron
  Vector2 chevron[] = {{x + float(sz * 1.25), y + sz}, {x, y}, {x - float(sz * 1.25), y + sz}};
  DrawTriangle(chevron[0], chevron[1], chevron[2], {201, 34, 49, fillAlpha});

}

void HudView::drawDriverState(const UIState *s) {
  const UIScene &scene = s->scene;

  // base icon
  int offset = UI_BORDER_SIZE + btn_size / 2;
  int x = rightHandDM ? GetScreenWidth() - offset : offset;
  int y = GetScreenHeight() - offset;
  uint8_t opacity = dmActive ? 164 : 51;
  // drawCircleTexture(dm_texture, x, y, blackColor(70), opacity);
  drawCircleTexture(&dm_texture, x, y, opacity);

  // // face
  std::vector<Vector2> points(std::size(default_face_kpts_3d));
  for (int i = 0; i < points.size(); ++i) {
    const auto &v = scene.face_kpts_draw[i].v;
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
  Color arc_color = {(uint8_t)(0.545 - 0.445 * s->engaged() * 255),
                     (uint8_t)(0.545 + 0.4 * s->engaged() * 255),
                     (uint8_t)(0.545 - 0.285 * s->engaged() * 255),
                     (uint8_t)(0.4 * (1.0 - dm_fade_state) * 255)};
  float delta_x = -scene.driver_pose_sins[1] * arc_l / 2;
  // float delta_y = -scene.driver_pose_sins[0] * arc_l / 2;
  // QRectF(std::fmin(x + delta_x, x), y - arc_l / 2, fabs(delta_x), arc_l)
  DrawCircleSectorLines(Vector2{(float)(x + delta_x), (float)(y - arc_l)}, 1, (scene.driver_pose_sins[1]>0 ? 90 : -90) * 16, 180 * 16, 10, arc_color);
  // DrawArc()
  // painter.setPen(QPen(arc_color, arc_t_default+arc_t_extend*fmin(1.0, scene.driver_pose_diff[1] * 5.0), Qt::SolidLine, Qt::RoundCap));
  // painter.drawArc(QRectF(std::fmin(x + delta_x, x), y - arc_l / 2, fabs(delta_x), arc_l), (scene.driver_pose_sins[1]>0 ? 90 : -90) * 16, 180 * 16);
  // painter.setPen(QPen(arc_color, arc_t_default+arc_t_extend*fmin(1.0, scene.driver_pose_diff[0] * 5.0), Qt::SolidLine, Qt::RoundCap));
  // painter.drawArc(QRectF(x - arc_l / 2, std::fmin(y + delta_y, y), arc_l, fabs(delta_y)), (scene.driver_pose_sins[0]>0 ? 0 : 180) * 16, 180 * 16);
}
