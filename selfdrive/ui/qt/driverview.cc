#include <iostream>

#include "common/timing.h"
#include "common/swaglog.h"

#include "driverview.hpp"
#include "paint.hpp"

DriverView::~DriverView() {
  makeCurrent();
  doneCurrent();
}

void DriverView::initializeGL() {
  initializeOpenGLFunctions();
  timer = new QTimer(this);
  QObject::connect(timer, SIGNAL(timeout()), this, SLOT(update()));
  timer->start(0);
  printf("*******************wocao*******************\n\n");
//   ui_nvg_init(&QUIState::ui_state);
//   prev_draw_t = millis_since_boot();
}

void DriverView::update() {
  // Connecting to visionIPC requires opengl to be current
  if (!vision) {
    vision = std::make_unique<UIVision>(QUIState::ui_state.video_rect, UIVision::DRIVER_CAM, 1.0);
  }
makeCurrent();
  printf("*******************wocao2*******************\n\n");
  repaint();
}

void DriverView::paintGL() {
    
  vision->update();
  if (!vision->connected()) {
    // nvgTextAlign(s->vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    // ui_draw_text(s, s->viz_rect.centerX(), s->viz_rect.centerY(), "Please wait for camera to start", 40 * 2.5, COLOR_WHITE, "sans-bold");
  }

//   ui_draw_background(s);
//   ui_draw_vision_frame(s);
 UIState *s = &QUIState::ui_state;
  glViewport(0, 0, s->fb_w, s->fb_h);
  glEnable(GL_SCISSOR_TEST);
//   glViewport(s->video_rect.x, s->video_rect.y, s->video_rect.w, s->video_rect.h);
//   glScissor(s->viz_rect.x, s->viz_rect.y, s->viz_rect.w, s->viz_rect.h);
  vision->draw();
  glDisable(GL_SCISSOR_TEST);

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
//   glViewport(0, 0, s->fb_w, s->fb_h);
 
  // NVG drawing functions - should be no GL inside NVG frame
  nvgBeginFrame(s->vg, s->fb_w, s->fb_h, 1.0f);
  ui_draw_driver_view(s);
  nvgEndFrame(s->vg);
  glDisable(GL_BLEND);

//   ui_draw(&QUIState::ui_state, width(), height());
}
