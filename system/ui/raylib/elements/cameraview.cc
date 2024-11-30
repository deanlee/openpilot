#include "system/ui/raylib/elements/cameraview.h"

#include "common/util.h"
#ifdef __APPLE__
#include <OpenGL/gl3.h>
#else
#include <GLES3/gl3.h>
#endif
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
#include <cassert>
#ifdef QCOM2
const char frame_fragment_shader[] = R"(
  #version 300 es
  #extension GL_OES_EGL_image_external_essl3 : enable
  precision mediump float;
  uniform samplerExternalOES texture0;
  in vec2 vTexCoord;
  out vec4 colorOut;
  void main() {
    colorOut = texture(texture0, vTexCoord);
    // gamma to improve worst case visibility when dark
    colorOut.rgb = pow(colorOut.rgb, vec3(1.0/1.28));
  };
)";
#else
const char frame_fragment_shader[] = R"(
  #version 330 core
  in vec2 fragTexCoord;
  uniform sampler2D texture0;  // Y plane
  uniform sampler2D texture1;  // UV plane
  out vec4 fragColor;
  void main() {
    float y = texture(texture0, fragTexCoord).r;
    vec2 uv = texture(texture1, fragTexCoord).ra - 0.5;
    float r = y + 1.402 * uv.y;
    float g = y - 0.344 * uv.x - 0.714 * uv.y;
    float b = y + 1.772 * uv.x;
    fragColor = vec4(r, g, b, 1.0);
  }
)";
#endif

CameraView::CameraView(const std::string &name, VisionStreamType type) {
  client = std::make_unique<VisionIpcClient>(name, type, false);
  shader = LoadShaderFromMemory(NULL, frame_fragment_shader);
}

CameraView::~CameraView() {
  if (textureY.id) UnloadTexture(textureY);
  if (textureUV.id) UnloadTexture(textureUV);
  if (shader.id) UnloadShader(shader);
}

void CameraView::draw(const Rectangle &rec) {
  if (!ensureConnection()) return;

  auto buffer = client->recv(nullptr, 20);
  frame = buffer ? buffer : frame;
  if (!frame) return;




  // Calculate scaling factors to maintain aspect ratio
  float scale = std::min((float)rec.width / frame->width, (float)rec.height / frame->height);
  float x_offset = rec.x + (rec.width - (frame->width * scale)) / 2;
  float y_offset = rec.y + (rec.height - (frame->height * scale)) / 2;
  Rectangle src_rect = {0, 0, (float)frame->width, (float)frame->height};
  Rectangle dst_rect = {x_offset, y_offset, frame->width * scale, frame->height * scale};


#ifdef QCOM2
  glActiveTexture(GL_TEXTURE0);
  glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, egl_images[frame->idx]);
  assert(glGetError() == GL_NO_ERROR);
  BeginShaderMode(shader);
  // SetShaderValueTexture(shader, GetShaderLocation(shader, "texture1"), textureUV);
  DrawTexturePro(textureY, src_rect, dst_rect, Vector2{0, 0}, 0.0, WHITE);
  EndShaderMode();
#else
  UpdateTexture(textureY, frame->y);
  UpdateTexture(textureUV, frame->uv);
  BeginShaderMode(shader);
  SetShaderValueTexture(shader, GetShaderLocation(shader, "texture1"), textureUV);
  DrawTexturePro(textureY, src_rect, dst_rect, Vector2{0, 0}, 0.0, WHITE);
  EndShaderMode();
#endif
}

bool CameraView::ensureConnection() {
  if (!client->connected) {
    frame = nullptr;
    if (!client->connect(false)) return false;

#ifdef QCOM2
  EGLDisplay egl_display = eglGetCurrentDisplay();
  assert(egl_display != EGL_NO_DISPLAY);
  for (auto &pair : egl_images) {
    eglDestroyImageKHR(egl_display, pair.second);
  }
  egl_images.clear();

  for (int i = 0; i < client->num_buffers; i++) {  // import buffers into OpenGL
    int fd = dup(client->buffers[i].fd);  // eglDestroyImageKHR will close, so duplicate
    EGLint img_attrs[] = {
      EGL_WIDTH, (int)client->buffers[i].width,
      EGL_HEIGHT, (int)client->buffers[i].height,
      EGL_LINUX_DRM_FOURCC_EXT, DRM_FORMAT_NV12,
      EGL_DMA_BUF_PLANE0_FD_EXT, fd,
      EGL_DMA_BUF_PLANE0_OFFSET_EXT, 0,
      EGL_DMA_BUF_PLANE0_PITCH_EXT, (int)client->buffers[i].stride,
      EGL_DMA_BUF_PLANE1_FD_EXT, fd,
      EGL_DMA_BUF_PLANE1_OFFSET_EXT, (int)client->buffers[i].uv_offset,
      EGL_DMA_BUF_PLANE1_PITCH_EXT, (int)client->buffers[i].stride,
      EGL_NONE
    };
    egl_images[i] = eglCreateImageKHR(egl_display, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, 0, img_attrs);
    assert(eglGetError() == EGL_SUCCESS);
  }
#else
    if (textureY.id) UnloadTexture(textureY);
    if (textureUV.id) UnloadTexture(textureUV);
    // Create textures for Y and UV planes
    const auto &buf = client->buffers[0];
    textureY = LoadTextureFromImage(Image{nullptr, (int)buf.stride, (int)buf.height, 1, PIXELFORMAT_UNCOMPRESSED_GRAYSCALE});
    textureUV = LoadTextureFromImage(Image{nullptr, (int)buf.stride / 2, (int)buf.height / 2, 1, PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA});
#endif
  }
  return true;
}
