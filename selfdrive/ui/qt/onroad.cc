#include "selfdrive/ui/qt/onroad.h"

#include <QDebug>
#include <cmath>

#include "common/timing.h"
#include "selfdrive/ui/qt/util.h"
#ifdef ENABLE_MAPS
#include "selfdrive/ui/qt/maps/map.h"
#include "selfdrive/ui/qt/maps/map_helpers.h"
#endif

static void drawHudText(QPainter &p, int x, int y, const QString &text, int alpha = 255) {
  QFontMetrics fm(p.font());
  QRect init_rect = fm.boundingRect(text);
  QRect real_rect = fm.boundingRect(init_rect, 0, text);
  real_rect.moveCenter({x, y - real_rect.height() / 2});

  p.setPen(QColor(0xff, 0xff, 0xff, alpha));
  p.drawText(real_rect.x(), real_rect.bottom(), text);
}

OnroadWindow::OnroadWindow(QWidget *parent) : QWidget(parent) {
  QVBoxLayout *main_layout = new QVBoxLayout(this);
  main_layout->setMargin(bdr_s);
  QStackedLayout *stacked_layout = new QStackedLayout;
  stacked_layout->setStackingMode(QStackedLayout::StackAll);
  main_layout->addLayout(stacked_layout);

  nvg = new OnroadView(VISION_STREAM_ROAD, this);

  QWidget *split_wrapper = new QWidget;
  split = new QHBoxLayout(split_wrapper);
  split->setContentsMargins(0, 0, 0, 0);
  split->setSpacing(0);
  split->addWidget(nvg);

  if (getenv("DUAL_CAMERA_VIEW")) {
    CameraWidget *arCam = new CameraWidget("camerad", VISION_STREAM_ROAD, true, this);
    split->insertWidget(0, arCam);
  }

  if (getenv("MAP_RENDER_VIEW")) {
    CameraWidget *map_render = new CameraWidget("navd", VISION_STREAM_MAP, false, this);
    split->insertWidget(0, map_render);
  }

  stacked_layout->addWidget(split_wrapper);

  setAttribute(Qt::WA_OpaquePaintEvent);
  QObject::connect(uiState(), &UIState::uiUpdate, this, &OnroadWindow::updateState);
  QObject::connect(uiState(), &UIState::offroadTransition, this, &OnroadWindow::offroadTransition);
}

void OnroadWindow::updateState(const UIState &s) {
  QColor bgColor = bg_colors[s.status];
  Alert alert = Alert::get(*(s.sm), s.scene.started_frame);
  if (s.sm->updated("controlsState") || !alert.equal({})) {
    if (alert.type == "controlsUnresponsive") {
      bgColor = bg_colors[STATUS_ALERT];
    } else if (alert.type == "controlsUnresponsivePermanent") {
      bgColor = bg_colors[STATUS_DISENGAGED];
    }
    // alerts->updateAlert(alert, bgColor);
  }

  if (s.scene.map_on_left) {
    split->setDirection(QBoxLayout::LeftToRight);
  } else {
    split->setDirection(QBoxLayout::RightToLeft);
  }

  // nvg->updateState(s);

  if (bg != bgColor) {
    // repaint border
    // bg = bgColor;
    // update();
  }
}

void OnroadWindow::mousePressEvent(QMouseEvent *e) {
  if (map != nullptr) {
    bool sidebarVisible = geometry().x() > 0;
    map->setVisible(!sidebarVisible && !map->isVisible());
  }
  // propagation event to parent(HomeWindow)
  QWidget::mousePressEvent(e);
}

void OnroadWindow::offroadTransition(bool offroad) {
#ifdef ENABLE_MAPS
  if (!offroad) {
    if (map == nullptr && (uiState()->prime_type || !MAPBOX_TOKEN.isEmpty())) {
      MapWindow *m = new MapWindow(get_mapbox_settings());
      map = m;

      QObject::connect(uiState(), &UIState::offroadTransition, m, &MapWindow::offroadTransition);

      m->setFixedWidth(topWidget(this)->width() / 2);
      split->insertWidget(0, m);

      // Make map visible after adding to split
      m->offroadTransition(offroad);
    }
  }
#endif

  // alerts->updateAlert({}, bg);
}

void OnroadWindow::paintEvent(QPaintEvent *event) {
  QPainter p(this);
  p.fillRect(rect(), QColor(bg.red(), bg.green(), bg.blue(), 255));
}

// ***** onroad widgets *****

// OnroadAlerts

// OnroadAlerts
void OnroadAlerts::update(const Alert &a, const QColor &color) {
  setVisible(a.size != cereal::ControlsState::AlertSize::NONE);
  if (!alert.equal(a) || color != bg) {
    alert = a;
    bg = color;
    if (!isVisible()) return;

    int h = 0;
    if (alert.size == cereal::ControlsState::AlertSize::SMALL)
      h = 271;
    else if (alert.size == cereal::ControlsState::AlertSize::MID)
      h = 420;
    else if (alert.size == cereal::ControlsState::AlertSize::FULL)
      h = scene()->sceneRect().height();

    setRect(0, 0, scene()->sceneRect().width(), h);
    setPos(0, scene()->sceneRect().bottom() - h);
    QGraphicsItem::update();
  }
}

void OnroadAlerts::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
  QRect r = boundingRect().toRect();
  QPainter &p = *painter;

  // draw background + gradient
  p.setPen(Qt::NoPen);
  p.setCompositionMode(QPainter::CompositionMode_SourceOver);

  p.setBrush(QBrush(bg));
  p.drawRect(r);

  QLinearGradient g(0, r.y(), 0, r.bottom());
  g.setColorAt(0, QColor::fromRgbF(0, 0, 0, 0.05));
  g.setColorAt(1, QColor::fromRgbF(0, 0, 0, 0.35));

  p.setCompositionMode(QPainter::CompositionMode_DestinationOver);
  p.setBrush(QBrush(g));
  p.fillRect(r, g);
  p.setCompositionMode(QPainter::CompositionMode_SourceOver);

  // text
  const QPoint c = r.center();
  p.setPen(QColor(0xff, 0xff, 0xff));
  p.setRenderHint(QPainter::TextAntialiasing);
  if (alert.size == cereal::ControlsState::AlertSize::SMALL) {
    configFont(p, "Inter", 74, "SemiBold");
    p.drawText(r, Qt::AlignCenter, alert.text1);
  } else if (alert.size == cereal::ControlsState::AlertSize::MID) {
    configFont(p, "Inter", 88, "Bold");
    p.drawText(QRect(0, c.y() - 125, r.width(), 150), Qt::AlignHCenter | Qt::AlignTop, alert.text1);
    configFont(p, "Inter", 66, "Regular");
    p.drawText(QRect(0, c.y() + 21, r.width(), 90), Qt::AlignHCenter, alert.text2);
  } else if (alert.size == cereal::ControlsState::AlertSize::FULL) {
    bool l = alert.text1.length() > 15;
    configFont(p, "Inter", l ? 132 : 177, "Bold");
    p.drawText(QRect(0, r.y() + (l ? 240 : 270), r.width(), 600), Qt::AlignHCenter | Qt::TextWordWrap, alert.text1);
    configFont(p, "Inter", 88, "Regular");
    p.drawText(QRect(0, r.height() - (l ? 361 : 420), r.width(), 300), Qt::AlignHCenter | Qt::TextWordWrap, alert.text2);
  }
}

ExperimentalButton::ExperimentalButton(QGraphicsItem *parent) : QGraphicsItem(parent) {
  setVisible(false);
  // setCheckable(true);

  params = Params();
  engage_img = loadPixmap("../assets/img_chffr_wheel.png", {img_size, img_size});
  experimental_img = loadPixmap("../assets/img_experimental.svg", {img_size, img_size});

  // QObject::connect(this, &QPushButton::toggled, [=](bool checked) {
  //   params.putBool("ExperimentalMode", checked);
  // });
}

void ExperimentalButton::updateState(const UIState &s) {
  const SubMaster &sm = *(s.sm);

  // button is "visible" if engageable or enabled
  const auto cs = sm["controlsState"].getControlsState();
  setVisible(cs.getEngageable() || cs.getEnabled());

  // button is "checked" if experimental mode is enabled
  // setChecked(sm["controlsState"].getControlsState().getExperimentalMode());

  // disable button when experimental mode is not available, or has not been confirmed for the first time
  // const auto cp = sm["carParams"].getCarParams();
  // const bool experimental_mode_available = cp.getExperimentalLongitudinalAvailable() ? params.getBool("ExperimentalLongitudinalEnabled") : cp.getOpenpilotLongitudinalControl();
  // setEnabled(params.getBool("ExperimentalModeConfirmed") && experimental_mode_available);
  // QGraphicsItem::update();
}

void ExperimentalButton::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
  QPainter &p = *painter;
  p.setRenderHint(QPainter::Antialiasing);

  QPoint center(btn_size / 2, btn_size / 2);
  // QPixmap img = isChecked() ? experimental_img : engage_img;
  QPixmap img = experimental_img;

  p.setOpacity(1.0);
  p.setPen(Qt::NoPen);
  p.setBrush(QColor(0, 0, 0, 166));
  p.drawEllipse(center, btn_size / 2, btn_size / 2);
  // p.setOpacity(isDown() ? 0.8 : 1.0);
  p.drawPixmap((btn_size - img_size) / 2, (btn_size - img_size) / 2, img);
}

void OnroadScene::updateState(const UIState &s) {
  max_speed->updateState(s);
  current_speed->updateState(s);
  // driver_state->updateState(s);
  experimental_btn->updateState(s);
  speed_limit->updateState(s);
}

void DriverStateItem::drawIcon(QPainter &p, int x, int y, QPixmap &img, QBrush bg, float opacity) {
  p.setOpacity(1.0);  // bg dictates opacity of ellipse
  p.setPen(Qt::NoPen);
  p.setBrush(bg);
  p.drawEllipse(x - btn_size / 2, y - btn_size / 2, btn_size, btn_size);
  p.setOpacity(opacity);
  p.drawPixmap(x - img.size().width() / 2, y - img.size().height() / 2, img);
}

void OnroadView::drawLaneLines(QPainter &painter, const UIState *s) {
  painter.save();

  const UIScene &scene = s->scene;
  SubMaster &sm = *(s->sm);

  // lanelines
  for (int i = 0; i < std::size(scene.lane_line_vertices); ++i) {
    painter.setBrush(QColor::fromRgbF(1.0, 1.0, 1.0, std::clamp<float>(scene.lane_line_probs[i], 0.0, 0.7)));
    painter.drawPolygon(scene.lane_line_vertices[i]);
  }

  // road edges
  for (int i = 0; i < std::size(scene.road_edge_vertices); ++i) {
    painter.setBrush(QColor::fromRgbF(1.0, 0, 0, std::clamp<float>(1.0 - scene.road_edge_stds[i], 0.0, 1.0)));
    painter.drawPolygon(scene.road_edge_vertices[i]);
  }

  // paint path
  QLinearGradient bg(0, height(), 0, height() / 4);
  float start_hue, end_hue;
  if (sm["controlsState"].getControlsState().getExperimentalMode()) {
    const auto &acceleration = sm["modelV2"].getModelV2().getAcceleration();
    float acceleration_future = 0;
    if (acceleration.getZ().size() > 16) {
      acceleration_future = acceleration.getX()[16];  // 2.5 seconds
    }
    start_hue = 60;
    // speed up: 120, slow down: 0
    end_hue = fmax(fmin(start_hue + acceleration_future * 45, 148), 0);

    // FIXME: painter.drawPolygon can be slow if hue is not rounded
    end_hue = int(end_hue * 100 + 0.5) / 100;

    bg.setColorAt(0.0, QColor::fromHslF(start_hue / 360., 0.97, 0.56, 0.4));
    bg.setColorAt(0.5, QColor::fromHslF(end_hue / 360., 1.0, 0.68, 0.35));
    bg.setColorAt(1.0, QColor::fromHslF(end_hue / 360., 1.0, 0.68, 0.0));
  } else {
    bg.setColorAt(0.0, QColor::fromHslF(148 / 360., 0.94, 0.51, 0.4));
    bg.setColorAt(0.5, QColor::fromHslF(112 / 360., 1.0, 0.68, 0.35));
    bg.setColorAt(1.0, QColor::fromHslF(112 / 360., 1.0, 0.68, 0.0));
  }

  painter.setBrush(bg);
  painter.drawPolygon(scene.track_vertices);

  painter.restore();
}

void DriverStateItem::updateState(const UIState &s) {
  dm_img = loadPixmap("../assets/img_driver_face.png", {img_size + 5, img_size + 5});
  // update DM icons at 2Hz
  if (s.sm->frame % (UI_FREQ / 2) == 0) {
    auto dm = (*s.sm)["driverMonitoringState"].getDriverMonitoringState();
    dmActive = dm.getIsActiveMode();
    rightHandDM = dm.getIsRHD();
    QGraphicsItem::update();
  }

  // DM icon transition
  dm_fade_state = fmax(0.0, fmin(1.0, dm_fade_state + 0.2 * (0.5 - (float)(dmActive))));
}

void DriverStateItem::paint(QPainter *p, const QStyleOptionGraphicsItem *option, QWidget *widget) {
  auto s = uiState();
  const UIScene &scene = s->scene;
  QPainter &painter = *p;
  painter.save();

  // base icon
  int x = rightHandDM ? boundingRect().right() - (btn_size - 24) / 2 - (bdr_s * 2) : (btn_size - 24) / 2 + (bdr_s * 2);
  int y = boundingRect().bottom() - footer_h / 2;
  float opacity = dmActive ? 0.65 : 0.2;
  drawIcon(painter, x, y, dm_img, blackColor(0), opacity);

  // circle background
  painter.setOpacity(1.0);
  painter.setPen(Qt::NoPen);
  painter.setBrush(blackColor(70));
  painter.drawEllipse(x - btn_size / 2, y - btn_size / 2, btn_size, btn_size);

  // face
  QPointF face_kpts_draw[std::size(default_face_kpts_3d)];
  float kp;
  for (int i = 0; i < std::size(default_face_kpts_3d); ++i) {
    kp = (scene.face_kpts_draw[i].v[2] - 8) / 120 + 1.0;
    face_kpts_draw[i] = QPointF(scene.face_kpts_draw[i].v[0] * kp + x, scene.face_kpts_draw[i].v[1] * kp + y);
  }

  painter.setPen(QPen(QColor::fromRgbF(1.0, 1.0, 1.0, opacity), 5.2, Qt::SolidLine, Qt::RoundCap));
  painter.drawPolyline(face_kpts_draw, std::size(default_face_kpts_3d));

  // tracking arcs
  const int arc_l = 133;
  const float arc_t_default = 6.7;
  const float arc_t_extend = 12.0;
  QColor arc_color = QColor::fromRgbF(0.09, 0.945, 0.26, 0.4 * (1.0 - dm_fade_state) * (s->engaged()));
  float delta_x = -scene.driver_pose_sins[1] * arc_l / 2;
  float delta_y = -scene.driver_pose_sins[0] * arc_l / 2;
  painter.setPen(QPen(arc_color, arc_t_default + arc_t_extend * fmin(1.0, scene.driver_pose_diff[1] * 5.0), Qt::SolidLine, Qt::RoundCap));
  painter.drawArc(QRectF(std::fmin(x + delta_x, x), y - arc_l / 2, fabs(delta_x), arc_l), (scene.driver_pose_sins[1] > 0 ? 90 : -90) * 16, 180 * 16);
  painter.setPen(QPen(arc_color, arc_t_default + arc_t_extend * fmin(1.0, scene.driver_pose_diff[0] * 5.0), Qt::SolidLine, Qt::RoundCap));
  painter.drawArc(QRectF(x - arc_l / 2, std::fmin(y + delta_y, y), arc_l, fabs(delta_y)), (scene.driver_pose_sins[0] > 0 ? 0 : 180) * 16, 180 * 16);

  painter.restore();
}

void OnroadView::drawLead(QPainter &painter, const cereal::RadarState::LeadData::Reader &lead_data, const QPointF &vd) {
  painter.save();

  const float speedBuff = 10.;
  const float leadBuff = 40.;
  const float d_rel = lead_data.getDRel();
  const float v_rel = lead_data.getVRel();

  float fillAlpha = 0;
  if (d_rel < leadBuff) {
    fillAlpha = 255 * (1.0 - (d_rel / leadBuff));
    if (v_rel < 0) {
      fillAlpha += 255 * (-1 * (v_rel / speedBuff));
    }
    fillAlpha = (int)(fmin(fillAlpha, 255));
  }

  float sz = std::clamp((25 * 30) / (d_rel / 3 + 30), 15.0f, 30.0f) * 2.35;
  float x = std::clamp((float)vd.x(), 0.f, width() - sz / 2);
  float y = std::fmin(height() - sz * .6, (float)vd.y());

  float g_xo = sz / 5;
  float g_yo = sz / 10;

  QPointF glow[] = {{x + (sz * 1.35) + g_xo, y + sz + g_yo}, {x, y - g_yo}, {x - (sz * 1.35) - g_xo, y + sz + g_yo}};
  painter.setBrush(QColor(218, 202, 37, 255));
  painter.drawPolygon(glow, std::size(glow));

  // chevron
  QPointF chevron[] = {{x + (sz * 1.25), y + sz}, {x, y}, {x - (sz * 1.25), y + sz}};
  painter.setBrush(redColor(fillAlpha));
  painter.drawPolygon(chevron, std::size(chevron));

  painter.restore();
}

OnroadView::OnroadView(VisionStreamType type, QWidget *parent) : QGraphicsView(parent) {
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setFrameStyle(0);
  
  
  cam_widget = new CameraWidget("camerad", type, true, this);
  setViewport(cam_widget);
  // setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
  
  onroad_scene = new OnroadScene(this);
  setScene(onroad_scene);

  // QObject::connect(uiState(), &UIState::offroadTransition, this, &OnroadGraphicsView::offroadTransition);
}

void OnroadView::resizeEvent(QResizeEvent *event) {
  QRect rc(QRect(QPoint(0, 0), event->size()));
  onroad_scene->setGeometry(rc);
  // camera_view->updateFrameMat(event->size().width(), event->size().height());
  QGraphicsView::resizeEvent(event);
}
#include <QPaintEngine>
void OnroadView::drawBackground(QPainter *painter, const QRectF &rect) {
  static double t1 = millis_since_boot();
  double t2 = millis_since_boot();
  qWarning() << t2 - t1;
  t1 = t2;
  auto s = uiState();
  auto &sm = *(s->sm);
  s->scene.wide_cam = false;
  if (s->scene.calibration_valid) {
    auto calib = s->scene.view_from_calib;
    cam_widget->updateCalibration(calib);
  } else {
    cam_widget->updateCalibration(DEFAULT_CALIBRATION);
  }
  // qWarning()<<painter->device()->paintEngine()->type() << "here";
  // cam_widget->setFrameId(model.getFrameId());
  // painter->beginNativePainting();
  cam_widget->paintGL();
  // painter->endNativePainting();

  const cereal::RadarState::Reader &radar_state = sm["radarState"].getRadarState();
  if (s->worldObjectsVisible()) {
    if (sm.rcv_frame("modelV2") > s->scene.started_frame) {
      update_model(s, sm["modelV2"].getModelV2(), sm["uiPlan"].getUiPlan());
      if (sm.rcv_frame("radarState") > s->scene.started_frame) {
        update_leads(s, radar_state, sm["modelV2"].getModelV2().getPosition());
      }
    }

    drawLaneLines(*painter, s);

    if (s->scene.longitudinal_control) {
      auto lead_one = radar_state.getLeadOne();
      auto lead_two = radar_state.getLeadTwo();
      if (lead_one.getStatus()) {
        drawLead(*painter, lead_one, s->scene.lead_vertices[0]);
      }
      if (lead_two.getStatus() && (std::abs(lead_one.getDRel() - lead_two.getDRel()) > 3.0)) {
        drawLead(*painter, lead_two, s->scene.lead_vertices[1]);
      }
    }
  }
  // QGraphicsView::drawBackground(painter, rect);
}

void OnroadView::updateFrameMat() {
}

OnroadScene::OnroadScene(QObject *parent) : QGraphicsScene(parent) {
  // QLinearGradient bg(0, header_h - (header_h / 2.5), 0, header_h);
  // bg.setColorAt(0, QColor::fromRgbF(0, 0, 0, 0.45));
  // bg.setColorAt(1, QColor::fromRgbF(0, 0, 0, 0));
  // header = addRect({}, Qt::NoPen, bg);

  addItem(max_speed = new MaxSpeedItem);
  addItem(current_speed = new CurrentSpeedItem);
  addItem(experimental_btn = new ExperimentalButton);
  addItem(driver_state = new DriverStateItem);
  addItem(alerts = new OnroadAlerts);
  addItem(speed_limit = new SpeedLimitItem);

  for (auto item : items()) {
    item->setFlag(QGraphicsItem::ItemIgnoresTransformations);
    item->setCacheMode(QGraphicsItem::DeviceCoordinateCache);
  }
  QObject::connect(uiState(), &UIState::uiUpdate, this, &OnroadScene::updateState);
}

void OnroadScene::setGeometry(const QRectF &rect) {
  setSceneRect(rect);

  max_speed->setPos(bdr_s * 2, bdr_s * 1.5);
  current_speed->setPos((rect.width() / 2 - current_speed->boundingRect().width() / 2), rect.top());
  experimental_btn->setPos(rect.right() - experimental_btn->boundingRect().width() - bdr_s * 2.0, bdr_s * 1.5);
  driver_state->setPos(bdr_s * 2, rect.bottom() - footer_h / 2 - driver_state->boundingRect().width() / 2);
  speed_limit->setPos(bdr_s * 2, 200);
  alerts->setPos(0, rect.bottom() - alerts->rect().height());
  alerts->setRect(0, 0, rect.width(), alerts->rect().height());
  // header->setRect(0, 0, rect.width(), header_h);
}

// SpeedLimitItem

void SpeedLimitItem::updateState(const UIState &s) {
  const bool nav_alive = s.sm->alive("navInstruction") && (*s.sm)["navInstruction"].getValid();
  auto nav = (*s.sm)["navInstruction"].getNavInstruction();
  float speedLimit = nav_alive ? nav.getSpeedLimit() : 0.0;
  speedLimit *= (s.scene.is_metric ? MS_TO_KPH : MS_TO_MPH);
  speedLimitStr = (speedLimit > 1) ? QString::number(std::nearbyint(speedLimit)) : "–";
  auto speed_limit_sign = nav.getSpeedLimitSign();
  has_us_speed_limit = nav_alive && speed_limit_sign == cereal::NavInstruction::SpeedLimitSign::MUTCD;
  has_eu_speed_limit = nav_alive && speed_limit_sign == cereal::NavInstruction::SpeedLimitSign::VIENNA;
  has_us_speed_limit = true;
  update();
}

QRectF SpeedLimitItem::boundingRect() const {
  return {0, 0, 186, 186};
}

void SpeedLimitItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
  // // US/Canada (MUTCD style) sign
  auto &p = *painter;
  if (has_us_speed_limit) {
    const int border_width = 6;
    const int sign_width = boundingRect().width();
    const int sign_height = 186;

    // White outer square
    QRect sign_rect_outer = boundingRect().toRect();
    p.setPen(Qt::NoPen);
    p.setBrush(whiteColor());
    p.drawRoundedRect(sign_rect_outer, 24, 24);

    // Smaller white square with black border
    QRect sign_rect(sign_rect_outer.left() + 1.5 * border_width, sign_rect_outer.top() + 1.5 * border_width, sign_width - 3 * border_width, sign_height - 3 * border_width);
    p.setPen(QPen(blackColor(), border_width));
    p.setBrush(whiteColor());
    p.drawRoundedRect(sign_rect, 16, 16);

    // "SPEED"
    configFont(p, "Inter", 28, "SemiBold");
    QRect text_speed_rect = getTextRect(p, Qt::AlignCenter, QObject::tr("SPEED"));
    text_speed_rect.moveCenter({sign_rect_outer.center().x(), 0});
    text_speed_rect.moveTop(sign_rect_outer.top() + 22);
    p.drawText(text_speed_rect, Qt::AlignCenter, QObject::tr("SPEED"));

    // "LIMIT"
    QRect text_limit_rect = getTextRect(p, Qt::AlignCenter, QObject::tr("LIMIT"));
    text_limit_rect.moveCenter({sign_rect_outer.center().x(), 0});
    text_limit_rect.moveTop(sign_rect_outer.top() + 51);
    p.drawText(text_limit_rect, Qt::AlignCenter, QObject::tr("LIMIT"));

    // Speed limit value
    configFont(p, "Inter", 70, "Bold");
    QRect speed_limit_rect = getTextRect(p, Qt::AlignCenter, speedLimitStr);
    speed_limit_rect.moveCenter({sign_rect_outer.center().x(), 0});
    speed_limit_rect.moveTop(sign_rect_outer.top() + 85);
    p.drawText(speed_limit_rect, Qt::AlignCenter, speedLimitStr);
  }

  // EU (Vienna style) sign
  if (has_eu_speed_limit) {
    int outer_radius = 176 / 2;
    int inner_radius_1 = outer_radius - 6; // White outer border
    int inner_radius_2 = inner_radius_1 - 20; // Red circle

    // Draw white circle with red border
    QPoint center = boundingRect().center().toPoint();
    p.setPen(Qt::NoPen);
    p.setBrush(whiteColor());
    p.drawEllipse(center, outer_radius, outer_radius);
    p.setBrush(QColor(255, 0, 0, 255));
    p.drawEllipse(center, inner_radius_1, inner_radius_1);
    p.setBrush(whiteColor());
    p.drawEllipse(center, inner_radius_2, inner_radius_2);

    // Speed limit value
    int font_size = (speedLimitStr.size() >= 3) ? 60 : 70;
    configFont(p, "Inter", font_size, "Bold");
    QRect speed_limit_rect = getTextRect(p, Qt::AlignCenter, speedLimitStr);
    speed_limit_rect.moveCenter(center);
    p.setPen(blackColor());
    p.drawText(speed_limit_rect, Qt::AlignCenter, speedLimitStr);
  }
}

// MaxSpeedItem
void MaxSpeedItem::updateState(const UIState &s) {
  const int SET_SPEED_NA = 255;
  const SubMaster &sm = *(s.sm);

  const bool cs_alive = sm.alive("controlsState");
  const auto cs = sm["controlsState"].getControlsState();

  // Handle older routes where vCruiseCluster is not set
  float v_cruise = cs.getVCruiseCluster() == 0.0 ? cs.getVCruise() : cs.getVCruiseCluster();
  float set_speed = cs_alive ? v_cruise : SET_SPEED_NA;
  bool cruise_set = set_speed > 0 && (int)set_speed != SET_SPEED_NA;
  if (cruise_set && !s.scene.is_metric) {
    set_speed *= KM_TO_MILE;
  }
  // auto speed = Qstring::number((int)set_speed);
  QString setSpeedStr = cruise_set ? QString::number(std::nearbyint(set_speed)) : "–";
  if (setSpeedStr != maxSpeed || cruise_set != is_cruise_set) {
    maxSpeed = setSpeedStr;
    is_cruise_set = cruise_set;
    QGraphicsItem::update();
  }
}
#include <QPaintEngine>
void MaxSpeedItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
  static int i = 0;
  qWarning()<<painter->device()->paintEngine()->type() << ++i;
  auto &p = *painter;
  // assert(p.device()->paintEngine()->type() == QPaintEngine::OpenGL2);
  const QRect rc = boundingRect().toRect();
  p.setRenderHint(QPainter::Antialiasing);
  p.setPen(QPen(QColor(0xff, 0xff, 0xff, 100), 10));
  p.setBrush(QColor(0, 0, 0, 100));
  p.drawRoundedRect(rc.adjusted(10, 10, -10, -10), 20, 20);
  p.setPen(Qt::NoPen);

  configFont(p, "Open Sans", 48, "Regular");
  drawHudText(p, rc.center().x(), 118 - bdr_s * 1.5, "MAX", is_cruise_set ? 200 : 100);
  if (is_cruise_set) {
    configFont(p, "Open Sans", 88, is_cruise_set ? "Bold" : "SemiBold");
    drawHudText(p, rc.center().x(), 212 - bdr_s * 1.5, maxSpeed, 255);
  } else {
    configFont(p, "Open Sans", 80, "SemiBold");
    drawHudText(p, rc.center().x(), 212 - bdr_s * 1.5, maxSpeed, 100);
  }
}
void CurrentSpeedItem::updateState(const UIState &s) {
  auto &sm = *(s.sm);
  // Handle older routes where vEgoCluster is not set
  float v_ego;
  auto cs = sm["carState"].getCarState();
  if (cs.getVEgoCluster() == 0.0 && !v_ego_cluster_seen) {
    v_ego = cs.getVEgo();
  } else {
    v_ego = cs.getVEgoCluster();
    v_ego_cluster_seen = true;
  }
  float cur_speed = sm.alive("controlsState") ? std::max<float>(0.0, v_ego) : 0.0;
  cur_speed *= s.scene.is_metric ? MS_TO_KPH : MS_TO_MPH;
  auto unit = s.scene.is_metric ? QObject::tr("km/h") : QObject::tr("mph");
  QString speedStr = QString::number(std::nearbyint(cur_speed));
  if (speedStr != speed || unit != speedUnit) {
    speed = speedStr;
    speedUnit = unit;
    QGraphicsItem::update();
  }
}

void CurrentSpeedItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
  auto &p = *painter;
  QRect rc = boundingRect().toRect();
  p.setRenderHint(QPainter::Antialiasing);
  configFont(p, "Open Sans", 176, "Bold");
  drawHudText(p, rc.center().x(), 210, speed);
  configFont(p, "Open Sans", 66, "Regular");
  drawHudText(p, rc.center().x(), 290, speedUnit, 200);
}
