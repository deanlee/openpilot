#include "selfdrive/ui/qt/widgets/cameraview.h"

#include <QDebug>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLFunctions_ES2>
#include "selfdrive/ui/qt/qt_window.h"

namespace {
const char frame_vertex_shader[] =
#ifdef NANOVG_GL3_IMPLEMENTATION
  "#version 150 core\n"
#else
  "#version 300 es\n"
#endif
  "in vec4 aPosition;\n"
  "in vec4 aTexCoord;\n"
  "uniform mat4 uTransform;\n"
  "out vec4 vTexCoord;\n"
  "void main() {\n"
  "  gl_Position = uTransform * aPosition;\n"
  "  vTexCoord = aTexCoord;\n"
  "}\n";

const char frame_fragment_shader[] =
#ifdef NANOVG_GL3_IMPLEMENTATION
  "#version 150 core\n"
#else
  "#version 300 es\n"
#endif
  "precision mediump float;\n"
  "uniform sampler2D uTexture;\n"
  "in vec4 vTexCoord;\n"
  "out vec4 colorOut;\n"
  "void main() {\n"
  "  colorOut = texture(uTexture, vTexCoord.xy);\n"
#ifdef QCOM
  "  vec3 dz = vec3(0.0627f, 0.0627f, 0.0627f);\n"
  "  colorOut.rgb = ((vec3(1.0f, 1.0f, 1.0f) - dz) * colorOut.rgb / vec3(1.0f, 1.0f, 1.0f)) + dz;\n"
#endif
  "}\n";

const mat4 device_transform = {{
  1.0,  0.0, 0.0, 0.0,
  0.0,  1.0, 0.0, 0.0,
  0.0,  0.0, 1.0, 0.0,
  0.0,  0.0, 0.0, 1.0,
}};

mat4 get_driver_view_transform() {
  const float driver_view_ratio = 1.333;
  mat4 transform;
  if (Hardware::TICI()) {
    // from dmonitoring.cc
    const int full_width_tici = 1928;
    const int full_height_tici = 1208;
    const int adapt_width_tici = 668;
    const int crop_x_offset = 32;
    const int crop_y_offset = -196;
    const float yscale = full_height_tici * driver_view_ratio / adapt_width_tici;
    const float xscale = yscale*(1080)/(2160)*full_width_tici/full_height_tici;
    transform = (mat4){{
      xscale,  0.0, 0.0, xscale*crop_x_offset/full_width_tici*2,
      0.0,  yscale, 0.0, yscale*crop_y_offset/full_height_tici*2,
      0.0,  0.0, 1.0, 0.0,
      0.0,  0.0, 0.0, 1.0,
    }};
  } else {
    // frame from 4/3 to 16/9 display
    transform = (mat4){{
      driver_view_ratio*(1080)/(1920),  0.0, 0.0, 0.0,
      0.0,  1.0, 0.0, 0.0,
      0.0,  0.0, 1.0, 0.0,
      0.0,  0.0, 0.0, 1.0,
    }};
  }
  return transform;
}

} // namespace
QSurfaceFormat getSurfaceFormat() {
  // Don't think this format needs to be in sync with that of the context
//   QSurfaceFormat fmt = QSurfaceFormat::defaultFormat();
//   fmt.setVersion(3, 3);
//   fmt.setRenderableType(QSurfaceFormat::OpenGL);
// #ifdef __APPLE__
//   // fmt.setRenderableType(QSurfaceFormat::OpenGL);
// #else
//   // fmt.setRenderableType(QSurfaceFormat::OpenGLES);
// #endif
//   fmt.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
//   fmt.setProfile(QSurfaceFormat::CoreProfile);
  // format.setSwapInterval(0); // run as fast as possible for whatever reason
   QSurfaceFormat format = QSurfaceFormat::defaultFormat();
    format.setVersion(3, 3);
    format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setProfile(QSurfaceFormat::CoreProfile);
  return format;
}
CameraViewWidget::CameraViewWidget(VisionStreamType stream_type) : QWindow() {
  setFlags(Qt::Widget);
  setSurfaceType(QSurface::OpenGLSurface);

  // setFormat(getSurfaceFormat());
  create();

  thread_ = new QThread();
  render_ = new Render(stream_type, this);
  render_->moveToThread(thread_);
  connect(thread_, &QThread::started, render_, &Render::start);
  connect(thread_, &QThread::finished, render_, &QObject::deleteLater);
  thread_->start();


}
void CameraViewWidget::mousePressEvent(QMouseEvent *ev) {
  qInfo() << "mosue here";
}

CameraViewWidget::~CameraViewWidget() {
  render_->exiting_ = true;
  thread_->quit();
  thread_->wait();
  delete thread_;
}

// class Render

Render::Render(VisionStreamType stream_type, CameraViewWidget *w) : stream_type(stream_type), glWindow_(w) {}

void Render::start() {
  initialize();
  while (!exiting_) {
    if (!glWindow_->isExposed()) {
      QThread::msleep(20);
      // qInfo("not exposed");
      continue;
    }
    QThread::msleep(20);

    context_->makeCurrent(glWindow_);
    // auto gl = context_->versionFunctions<QOpenGLFunctions_3_3_Core>();
    // // qInfo() << "here";
    // gl->glViewport(0, 0, glWindow_->width(), glWindow_->height());
    // // gl->glClearColor(.1f, .0f, .0f, .4f);
    // const QColor &color = bg_colors[STATUS_ENGAGED];
    // glClearColor(color.redF(), color.greenF(), color.blueF(), 1.0);
    // gl->glClear(GL_COLOR_BUFFER_BIT);

    // qInfo() << "draw" << glWindow_->width() << glWindow_->height();

    updateFrame();
    draw();
    context_->swapBuffers(glWindow_);
  }
}
void Render::initialize() {
  context_ = new QOpenGLContext;
  
  context_->setFormat(getSurfaceFormat());
  bool ret = context_->create();
  assert(ret);
  // qInfo() << "here1";
  // initializeOpenGLFunctions();
  // initializeOpenGLFunctions();
 
  context_->makeCurrent(glWindow_);
  // auto gl = context_->versionFunctions<QOpenGLFunctions_3_3_Core>();
  gl_shader = std::make_unique<GLShader>(frame_vertex_shader, frame_fragment_shader);
  GLint frame_pos_loc = glGetAttribLocation(gl_shader->prog, "aPosition");
  GLint frame_texcoord_loc = glGetAttribLocation(gl_shader->prog, "aTexCoord");

  auto [x1, x2, y1, y2] = stream_type == VISION_STREAM_RGB_FRONT ? std::tuple(0.f, 1.f, 1.f, 0.f) : std::tuple(1.f, 0.f, 1.f, 0.f);
  const uint8_t frame_indicies[] = {0, 1, 2, 0, 2, 3};
  const float frame_coords[4][4] = {
    {-1.0, -1.0, x2, y1}, //bl
    {-1.0,  1.0, x2, y2}, //tl
    { 1.0,  1.0, x1, y2}, //tr
    { 1.0, -1.0, x1, y1}, //br
  };

  glGenVertexArrays(1, &frame_vao);
  glBindVertexArray(frame_vao);
  glGenBuffers(1, &frame_vbo);
  glBindBuffer(GL_ARRAY_BUFFER, frame_vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(frame_coords), frame_coords, GL_STATIC_DRAW);
  glEnableVertexAttribArray(frame_pos_loc);
  glVertexAttribPointer(frame_pos_loc, 2, GL_FLOAT, GL_FALSE,
                        sizeof(frame_coords[0]), (const void *)0);
  glEnableVertexAttribArray(frame_texcoord_loc);
  glVertexAttribPointer(frame_texcoord_loc, 2, GL_FLOAT, GL_FALSE,
                        sizeof(frame_coords[0]), (const void *)(sizeof(float) * 2));
  glGenBuffers(1, &frame_ibo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, frame_ibo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(frame_indicies), frame_indicies, GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindVertexArray(0);

  if (stream_type == VISION_STREAM_RGB_FRONT) {
    frame_mat = matmul(device_transform, get_driver_view_transform());
  } else {
    auto intrinsic_matrix = stream_type == VISION_STREAM_RGB_WIDE ? ecam_intrinsic_matrix : fcam_intrinsic_matrix;
    float zoom_ = zoom / intrinsic_matrix.v[0];
    if (stream_type == VISION_STREAM_RGB_WIDE) {
      zoom_ *= 0.5;
    }
    float zx = zoom_ * 2 * intrinsic_matrix.v[2] / glWindow_->width();
    float zy = zoom_ * 2 * intrinsic_matrix.v[5] / glWindow_->height();

    const mat4 frame_transform = {{
      zx, 0.0, 0.0, 0.0,
      0.0, zy, 0.0, -y_offset / glWindow_->height() * 2,
      0.0, 0.0, 1.0, 0.0,
      0.0, 0.0, 0.0, 1.0,
    }};
    frame_mat = matmul(device_transform, frame_transform);
  }
  vipc_client = std::make_unique<VisionIpcClient>("camerad", stream_type, true);
}

void Render::render(bool cleanup) {
  // QOpenGLContext *ctx = glWindow_->context();
  // if (!ctx) {  // QOpenGLWidget not yet initialized
  //   return;
  // }
  // // move the context to this thread.
  // std::unique_lock lk(renderMutex_);
  // emit contextWanted();
  // grabCond_.wait(lk, [=] {
  //   return glWindow_->context()->thread() == QThread::currentThread() || exiting_;
  // });
  // if (exiting_) {
  //   return;
  // }

  // // Make the context (and an offscreen surface) current for this thread. The
  // // QOpenGLWidget's fbo is bound in the context.
  // glWindow_->makeCurrent();

  // if (!inited_) {
  //   inited_ = true;
  //   initialize();
  // }

  // if (cleanup) {
  //   vipc_client->connected = false;
  //   latest_frame = nullptr;
  // } else {
  //   updateFrame();
  // }
  // draw();

  // // context back to the gui thread.
  // glWindow_->doneCurrent();
  // glWindow_->context()->moveToThread(glWindow_->thread());
  // // Schedule composition.update() will be invoked on the gui thread.
  // QMetaObject::invokeMethod(glWindow_, "update");
}

void Render::updateFrame() {
  if (!vipc_client->connected && vipc_client->connect(false)) {
    // init vision
    for (int i = 0; i < vipc_client->num_buffers; i++) {
      texture[i].reset(new EGLImageTexture(&vipc_client->buffers[i]));

      glBindTexture(GL_TEXTURE_2D, texture[i]->frame_tex);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

      // BGR
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_BLUE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_GREEN);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);
      assert(glGetError() == GL_NO_ERROR);
    }
    latest_frame = nullptr;
  }

  if (vipc_client->connected) {
    VisionBuf *buf = vipc_client->recv();
    if (buf != nullptr) {
      latest_frame = buf;
    } else {
      LOGE("visionIPC receive timeout");
      QThread::msleep(20);
    }
  }
}

void Render::draw() {
  if (!latest_frame) {
    glClearColor(0, 0, 0, 1.0);
    glClear(GL_STENCIL_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
    return;
  }

  glViewport(0, 0, glWindow_->width(), glWindow_->height());

  glBindVertexArray(frame_vao);
  glActiveTexture(GL_TEXTURE0);

  glBindTexture(GL_TEXTURE_2D, texture[latest_frame->idx]->frame_tex);
  if (!Hardware::EON()) {
    // this is handled in ion on QCOM
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, latest_frame->width, latest_frame->height,
                 0, GL_RGB, GL_UNSIGNED_BYTE, latest_frame->addr);
  }

  glUseProgram(gl_shader->prog);
  glUniform1i(gl_shader->getUniformLocation("uTexture"), 0);
  glUniformMatrix4fv(gl_shader->getUniformLocation("uTransform"), 1, GL_TRUE, frame_mat.v);

  assert(glGetError() == GL_NO_ERROR);
  glEnableVertexAttribArray(0);
  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, (const void *)0);
  glDisableVertexAttribArray(0);
  glBindVertexArray(0);
}
