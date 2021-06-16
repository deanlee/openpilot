#pragma once

#include <memory>
#include <mutex>
#include <condition_variable>

#include <QOpenGLFunctions>
#include <QOpenGLWidget>
#include <QThread>
#include <QWindow>

#include "cereal/visionipc/visionipc_client.h"
#include "selfdrive/common/glutil.h"
#include "selfdrive/common/mat.h"
#include "selfdrive/common/visionimg.h"
#include "selfdrive/ui/ui.h"

class Render;
class CameraViewWidget : public QWindow {
  Q_OBJECT
public:
  explicit CameraViewWidget(VisionStreamType stream_type);
  ~CameraViewWidget();

  void mousePressEvent(QMouseEvent *ev);
// signals:
//  void frameUpdated();
//  void renderRequested(bool cleanup);

// public slots:
//   void moveContextToThread();

protected:
  // void paintEvent(QPaintEvent* event) override {}
  // void hideEvent(QHideEvent *event) override;
  QThread *thread_;
  Render *render_;
};

class Render : public QObject {
  Q_OBJECT
public:
  Render(VisionStreamType stream_type, CameraViewWidget *w); 
  ~Render() {
    glWindow_->destroy();
  }
  void render(bool cleanup);
  void start();

signals:
  void contextWanted();

private:
  void updateFrame();
  bool frameUpdated() const { return latest_frame != nullptr; };
  void initialize();
  void draw();
  bool inited_ = false;
  QOpenGLContext *context_;
  CameraViewWidget * glWindow_;
  std::mutex renderMutex_;
  std::condition_variable grabCond_;

  VisionBuf *latest_frame = nullptr;
  GLuint frame_vao, frame_vbo, frame_ibo;
  mat4 frame_mat;
  std::unique_ptr<VisionIpcClient> vipc_client;
  std::unique_ptr<EGLImageTexture> texture[UI_BUF_COUNT];
  std::unique_ptr<GLShader> gl_shader;

  VisionStreamType stream_type;
  bool exiting_ = false;
  friend class CameraViewWidget;
};
