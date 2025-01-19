#pragma once

#include <map>
#include <memory>
#include <set>
#include <string>

#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLWidget>

#ifdef QCOM2
#define EGL_EGLEXT_PROTOTYPES
#define EGL_NO_X11
#define GL_TEXTURE_EXTERNAL_OES 0x8D65
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <drm/drm_fourcc.h>
#endif

#include "msgq/visionipc/visionipc_client.h"
#include "selfdrive/ui/ui.h"

class CameraWidget : public QOpenGLWidget, protected QOpenGLFunctions {
  Q_OBJECT

public:
  explicit CameraWidget(std::string stream_name, VisionStreamType stream_type, QWidget* parent = nullptr);
  ~CameraWidget();
  void setBackgroundColor(const QColor &color) { bg = color; }
  void setStreamType(VisionStreamType type) { cur_stream_type = type; }
  VisionStreamType getStreamType() { return cur_stream_type; }
  void disconnect();

signals:
  void clicked();
  void vipcAvailableStreamsUpdated(std::set<VisionStreamType>);

protected:
  void paintGL() override;
  void initializeGL() override;
  void clearEGLImages();
  void mouseReleaseEvent(QMouseEvent *event) override { emit clicked(); }
  virtual mat4 calcFrameMatrix();
  VisionBuf *receiveFrame();
  void vipcConnected();

  int glWidth() const { return width() * devicePixelRatio(); }
  int glHeight() const { return height() * devicePixelRatio(); }

  GLuint frame_vao, frame_vbo, frame_ibo;
  GLuint textures[2];
  std::unique_ptr<QOpenGLShaderProgram> program;
  QColor bg = QColor("#000000");
#ifdef QCOM2
  std::map<int, EGLImageKHR> egl_images;
#endif
  std::string stream_name;
  int stream_width = 0;
  int stream_height = 0;
  int stream_stride = 0;
  VisionStreamType cur_stream_type;
  std::unique_ptr<VisionIpcClient> vipc_client;
  std::set<VisionStreamType> available_streams;
  VisionBuf *vision_buf = nullptr;
  uint32_t prev_frame_id = 0;
};

class AutoUpdateCameraWidget : public CameraWidget {
 public:
  AutoUpdateCameraWidget(std::string stream_name, VisionStreamType stream_type, QWidget *parent = nullptr)
      : CameraWidget(stream_name, stream_type, parent) {
    QTimer *timer = new QTimer(this);
    timer->callOnTimeout([this]() { update(); });
    timer->start(1000 / UI_FREQ);
  }
};
