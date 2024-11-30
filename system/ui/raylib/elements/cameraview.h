#pragma once

#include <memory>

#ifdef QCOM2
#define EGL_EGLEXT_PROTOTYPES
#define EGL_NO_X11
#define GL_TEXTURE_EXTERNAL_OES 0x8D65
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>  // For OpenGL ES extensions
#include <drm/drm_fourcc.h>
#endif

#include "msgq/visionipc/visionipc_client.h"
#include "third_party/raylib/include/raylib.h"

class CameraView {
public:
  CameraView(const std::string &name, VisionStreamType type);
  virtual ~CameraView();
  void draw(const Rectangle &rec);

protected:
  bool ensureConnection();

  std::unique_ptr<VisionIpcClient> client;
  Texture2D textureY = {};
  Texture2D textureUV = {};
  Shader shader = {};
  VisionBuf *frame = nullptr;
  #ifdef QCOM2
  std::map<int, EGLImageKHR> egl_images;
#endif

};
