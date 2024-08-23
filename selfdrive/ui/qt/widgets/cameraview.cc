#include "selfdrive/ui/qt/widgets/cameraview.h"
#include "common/util.h"
#include "msgq/visionipc/visionipc_client.h"

#include <raylib.h>

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

struct CameraView::Private {
  Texture2D textureY;
  Texture2D textureUV;
  Shader shader;
};

CameraView::CameraView(const std::string &stream_name, VisionStreamType stream_type, bool zoom) {
  priv = new Private();
  vipc_client = std::make_unique<VisionIpcClient>(stream_name, stream_type, false);
  priv->shader = LoadShaderFromMemory(NULL, frame_fragment_shader);
}

CameraView::~CameraView() {
  UnloadTexture(priv->textureY);
  UnloadTexture(priv->textureUV);
  UnloadShader(priv->shader);
  delete priv;
}

void CameraView::connect() {
  if (vipc_client->connect()) {
    // Get stream dimensions
    stream_width = vipc_client->buffers[0].width;
    stream_height = vipc_client->buffers[0].height;
    stream_stride = vipc_client->buffers[0].stride;
        // Create textures for Y and UV planes
    priv->textureY = LoadTextureFromImage({nullptr, stream_stride, stream_height, 1, PIXELFORMAT_UNCOMPRESSED_GRAYSCALE});
    priv->textureUV = LoadTextureFromImage({nullptr, stream_stride / 2, stream_height / 2, 1, PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA});

  }
}

void CameraView::draw() {
  if (!vipc_client->connected) {
    connect();
  }
  if (!stream_width) return;

  // Calculate scaling factors to maintain aspect ratio
  float scale = std::min((float)GetScreenWidth() / stream_width, (float)GetScreenHeight() / stream_height);
  float x_offset = (GetScreenWidth() - (stream_width * scale)) / 2;
  float y_offset = (GetScreenHeight() - (stream_height * scale)) / 2;
  Rectangle src_rect = {0, 0, (float)stream_width, (float)stream_height};
  Rectangle dst_rect = {x_offset, y_offset, stream_width * scale, stream_height * scale};

  auto buf = vipc_client->recv();
  if (buf) {
    UpdateTexture(priv->textureY, buf->y);
    UpdateTexture(priv->textureUV, buf->uv);
  }

  ClearBackground(BLACK);
    BeginShaderMode(priv->shader);
      SetShaderValueTexture(priv->shader, GetShaderLocation(priv->shader, "texture1"), priv->textureUV);
      DrawTexturePro(priv->textureY, src_rect, dst_rect, Vector2{0, 0}, 0.0, WHITE);
    EndShaderMode();
  DrawText("RAYLIB CAMERAVIEW", 10, 10, 20, WHITE);
}
