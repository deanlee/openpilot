#pragma once

#include <map>
#include <memory>
#include <set>

#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLWidget>
#include <QThread>

#ifdef QCOM2
#define EGL_EGLEXT_PROTOTYPES
#define EGL_NO_X11
#define GL_TEXTURE_EXTERNAL_OES 0x8D65
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <drm/drm_fourcc.h>
#endif

#include "msgq/visionipc/visionipc_client.h"
#include "system/camerad/cameras/camera_common.h"
#include "selfdrive/ui/ui.h"

class CameraWidget : public QOpenGLWidget, protected QOpenGLFunctions {
  Q_OBJECT

public:
  using QOpenGLWidget::QOpenGLWidget;
  explicit CameraWidget(std::string stream_name, VisionStreamType stream_type, bool zoom, QWidget* parent = nullptr);
  ~CameraWidget();
  void setBackgroundColor(const QColor &color) { bg = color; }
  void setStreamType(VisionStreamType type) { requested_stream_type = type; }
  VisionStreamType getStreamType() { return requested_stream_type; }

signals:
  void clicked();
  void vipcAvailableStreamsUpdated(std::set<VisionStreamType>);

protected:
  void paintGL() override;
  void initializeGL() override;
  void mouseReleaseEvent(QMouseEvent *event) override { emit clicked(); }
  virtual void updateFrameMat();
  void updateCalibration(const mat3 &calib) { calibration = calib; }
  void vipcConnected();
  VisionBuf *receiveFrame();
  int glWidth() { return width() * devicePixelRatio(); }
  int glHeight() { return height() * devicePixelRatio(); }

  bool zoomed_view;
  GLuint frame_vao, frame_vbo, frame_ibo;
  GLuint textures[2];
  mat4 frame_mat = {};
  std::unique_ptr<QOpenGLShaderProgram> program;
  QColor bg = QColor("#000000");
  VisionBuf *frame  = nullptr;

#ifdef QCOM2
  std::map<int, EGLImageKHR> egl_images;
#endif

  std::string stream_name;
  int stream_width = 0;
  int stream_height = 0;
  VisionStreamType requested_stream_type;
  std::set<VisionStreamType> available_streams;
  std::unique_ptr<VisionIpcClient> vipc_client;

  // Calibration
  float x_offset = 0;
  float y_offset = 0;
  float zoom = 1.0;
  mat3 calibration = DEFAULT_CALIBRATION;
  mat3 intrinsic_matrix = FCAM_INTRINSIC_MATRIX;
};
