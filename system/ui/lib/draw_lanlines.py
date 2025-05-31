import pyray as rl
import numpy as np
from typing import Any

MAX_GRADIENT_COLORS = 60  # 4 colors per gradient * 15 polygons
MAX_BATCH_POLYGONS = 15
MAX_BATCH_POINTS = 300

FRAGMENT_SHADER = """
#version 300 es
precision mediump float;

in vec2 fragTexCoord;
out vec4 finalColor;

uniform vec2 allPoints[300];           // Polygon points
uniform int polygonStarts[15];         // Start index per polygon
uniform int polygonCounts[15];         // Point count per polygon
uniform int polygonCount;              // Number of polygons
uniform vec4 solidColors[15];          // Solid colors
uniform int useGradientFlags[15];      // 1 for gradient, 0 for solid
uniform vec2 gradientStarts[15];       // Gradient start
uniform vec2 gradientEnds[15];         // Gradient end
uniform vec4 batchGradientColors[60];  // Up to 4 colors per gradient
uniform float batchGradientStops[60];  // Gradient stops
uniform int gradientColorCounts[15];   // Colors per gradient
uniform vec2 resolution;

// Find segment on a chain (left: y decreasing, right: y increasing)
bool findSegment1(int startIdx, int endIdx, float y, bool isLeft, out float x) {
  for (int j = startIdx; j < endIdx - 1; j++) {
    bool inRange = isLeft ?
      (allPoints[j].y >= y && allPoints[j + 1].y <= y) :
      (allPoints[j].y <= y && allPoints[j + 1].y >= y);
    if (inRange) {
      float t = (y - allPoints[j + (isLeft ? 1 : 0)].y) /
                (allPoints[j + (isLeft ? 0 : 1)].y - allPoints[j + (isLeft ? 1 : 0)].y + 0.001);
      x = mix(allPoints[j + (isLeft ? 1 : 0)].x, allPoints[j + (isLeft ? 0 : 1)].x, t);
      return true;
    }
  }
  return false;
}

// Binary search for segment on a chain (left: y decreasing, right: y increasing)
bool findSegment(int startIdx, int endIdx, float y, bool isLeft, out float x) {
  int count = endIdx - startIdx;
  if (count < 2) return false; // Need at least 2 points for a segment

  int low = startIdx;
  int high = endIdx - 2; // Last segment starts at endIdx - 2

  while (low <= high) {
    int mid = low + (high - low) / 2;
    float y0 = allPoints[mid].y;
    float y1 = allPoints[mid + 1].y;

    if (isLeft) {
      // Left chain: y0 >= y >= y1 (decreasing)
      if (y0 >= y && y1 <= y) {
        // Found segment, interpolate x
        float t = (y - y1) / (y0 - y1 + 0.001);
        x = mix(allPoints[mid + 1].x, allPoints[mid].x, t);
        return true;
      } else if (y0 < y) {
        high = mid - 1; // y too high, search lower indices
      } else {
        low = mid + 1;  // y too low, search higher indices
      }
    } else {
      // Right chain: y0 <= y <= y1 (increasing)
      if (y0 <= y && y1 >= y) {
        // Found segment, interpolate x
        float t = (y - y0) / (y1 - y0 + 0.001);
        x = mix(allPoints[mid].x, allPoints[mid + 1].x, t);
        return true;
      } else if (y0 > y) {
        high = mid - 1; // y too low, search lower indices
      } else {
        low = mid + 1;  // y too high, search higher indices
      }
    }
  }
  return false; // No segment found
}

// Distance to a line segment
float distanceToSegment(vec2 p, vec2 v, vec2 w) {
  vec2 vw = w - v;
  float l2 = dot(vw, vw);
  if (l2 < 0.0001) return length(p - v);
  float t = clamp(dot(p - v, vw) / l2, 0.0, 1.0);
  vec2 projection = v + t * vw;
  return length(p - projection);
}

// Signed distance to polygon
float distanceToPolygonEdge(vec2 p, int polyIndex) {
  int startIdx = polygonStarts[polyIndex];
  int pointCount = polygonCounts[polyIndex];
  float minDist = 1e10;

  // Check all edges
  for (int i = 0, j = pointCount - 1; i < pointCount; j = i++) {
    vec2 edge0 = allPoints[startIdx + j];
    vec2 edge1 = allPoints[startIdx + i];
    minDist = min(minDist, distanceToSegment(p, edge0, edge1));
  }

  // Point-in-polygon test
  float x_left, x_right;
  int leftStart = startIdx;
  int leftEnd = startIdx + pointCount / 2;
  int rightStart = leftEnd;
  int rightEnd = startIdx + pointCount;
  if (findSegment(leftStart, leftEnd, p.y, true, x_left) &&
      findSegment(rightStart, rightEnd, p.y, false, x_right)) {
    if (x_left < p.x && p.x < x_right) {
      return minDist; // Inside
    }
  }
  return -minDist; // Outside
}

// Qt-like linear gradient
vec4 getBatchGradientColor(vec2 pos, int polyIndex) {
  int colorStart = polyIndex * 4;
  int colorCount = gradientColorCounts[polyIndex];

  vec2 gradientDir = gradientEnds[polyIndex] - gradientStarts[polyIndex];
  float gradientLength = length(gradientDir);
  if (gradientLength < 0.001) return batchGradientColors[colorStart];

  vec2 normalizedDir = gradientDir / gradientLength;
  vec2 pointVec = pos - gradientStarts[polyIndex];
  float projection = dot(pointVec, normalizedDir);
  float t = clamp(projection / gradientLength, 0.0, 1.0);

  for (int i = 0; i < colorCount - 1; i++) {
    int stopIdx = colorStart + i;
    if (t >= batchGradientStops[stopIdx] && t <= batchGradientStops[stopIdx + 1]) {
      float segmentT = (t - batchGradientStops[stopIdx]) /
                       (batchGradientStops[stopIdx + 1] - batchGradientStops[stopIdx] + 0.001);
      return mix(batchGradientColors[stopIdx], batchGradientColors[stopIdx + 1], segmentT);
    }
  }
  return batchGradientColors[colorStart + colorCount - 1];
}

// Get color (solid or gradient)
vec4 getColor(int polyIndex) {
  if (useGradientFlags[polyIndex] == 1) {
    return getBatchGradientColor(fragTexCoord * resolution, polyIndex);
  }
  return solidColors[polyIndex];
}

void main() {
  vec2 pixel = fragTexCoord * resolution;
  vec4 finalResult = vec4(0.0);

  // Resolution-adaptive aaWidth
  vec2 pixelGrad = vec2(dFdx(pixel.x), dFdy(pixel.y));
  float pixelSize = length(pixelGrad);
  float aaWidth = max(0.5, pixelSize * 1.0); // ~2-pixel transition

  // Process polygons (non-overlapping)
  for (int i = polygonCount - 1; i >= 0; i--) {
    int startIdx = polygonStarts[i];
    int pointCount = polygonCounts[i];
    int leftStart = startIdx;
    int leftEnd = startIdx + pointCount / 2;
    int rightStart = leftEnd;
    int rightEnd = startIdx + pointCount;

    // Find chain segments
    float x_left, x_right;
    if (!findSegment(leftStart, leftEnd, pixel.y, true, x_left)) continue;
    if (!findSegment(rightStart, rightEnd, pixel.y, false, x_right)) continue;

    // Check if pixel is inside
    if (x_left < pixel.x && pixel.x < x_right) {
      // Fast but less accurate for curved lanes
      // sd = min(pixel.x - x_left, x_right - pixel.x)
      // Accurate signed distance
      float sd = distanceToPolygonEdge(pixel, i);
      // Optional: float sd = min(pixel.x - x_left, x_right - pixel.x); // Test for straight lines

      // Anti-aliasing
      float alpha = sd > aaWidth ? 1.0 : smoothstep(-aaWidth, aaWidth, sd);
      if (alpha <= 0.0) continue;

      // Apply color
      vec4 color = getColor(i);
      finalResult = vec4(color.rgb, alpha);
      break; // Non-overlapping
    }
  }

  finalColor = finalResult;
}
"""

# Default vertex shader
VERTEX_SHADER = """
#version 300 es
in vec3 vertexPosition;
in vec2 vertexTexCoord;
out vec2 fragTexCoord;
uniform mat4 mvp;

void main() {
  fragTexCoord = vertexTexCoord;
  gl_Position = mvp * vec4(vertexPosition, 1.0);
}
"""

UNIFORM_INT = rl.ShaderUniformDataType.SHADER_UNIFORM_INT
UNIFORM_FLOAT = rl.ShaderUniformDataType.SHADER_UNIFORM_FLOAT
UNIFORM_VEC2 = rl.ShaderUniformDataType.SHADER_UNIFORM_VEC2
UNIFORM_VEC4 = rl.ShaderUniformDataType.SHADER_UNIFORM_VEC4


class ShaderState:
  _instance: Any = None

  @classmethod
  def get_instance(cls):
    if cls._instance is None:
      cls._instance = cls()
    return cls._instance

  def __init__(self):
    if ShaderState._instance is not None:
      raise Exception("This class is a singleton. Use get_instance() instead.")

    self.initialized = False
    self.shader = None
    self.white_texture = None

    # Shader uniform locations
    self.locations = {
      'mvp': None,
      'resolution': None,
      # Batch polygon uniforms
      'allPoints': None,
      'polygonStarts': None,
      'polygonCounts': None,
      'polygonCount': None,
      'solidColors': None,
      'useGradientFlags': None,
      'gradientStarts': None,
      'gradientEnds': None,
      'batchGradientColors': None,
      'batchGradientStops': None,
      'gradientColorCounts': None,
    }

    # Batch FFI objects
    self.resolution_ptr = rl.ffi.new("float[]", [0.0, 0.0])
    self.all_points_ptr = rl.ffi.new("float[]", MAX_BATCH_POINTS * 2)
    self.polygon_starts_ptr = rl.ffi.new("int[]", MAX_BATCH_POLYGONS)
    self.polygon_counts_ptr = rl.ffi.new("int[]", MAX_BATCH_POLYGONS)
    self.polygon_count_ptr = rl.ffi.new("int[]", [0])
    self.solid_colors_ptr = rl.ffi.new("float[]", MAX_BATCH_POLYGONS * 4)
    self.use_gradient_flags_ptr = rl.ffi.new("int[]", MAX_BATCH_POLYGONS)
    self.batch_gradient_starts_ptr = rl.ffi.new("float[]", MAX_BATCH_POLYGONS * 2)
    self.batch_gradient_ends_ptr = rl.ffi.new("float[]", MAX_BATCH_POLYGONS * 2)
    self.batch_gradient_colors_ptr = rl.ffi.new("float[]", MAX_GRADIENT_COLORS * 4)
    self.batch_gradient_stops_ptr = rl.ffi.new("float[]", MAX_GRADIENT_COLORS)
    self.gradient_color_counts_ptr = rl.ffi.new("int[]", MAX_BATCH_POLYGONS)

  def initialize(self):
    if self.initialized:
      return

    self.shader = rl.load_shader_from_memory(VERTEX_SHADER, FRAGMENT_SHADER)

    # Create and cache white texture
    white_img = rl.gen_image_color(2, 2, rl.WHITE)
    self.white_texture = rl.load_texture_from_image(white_img)
    rl.set_texture_filter(self.white_texture, rl.TEXTURE_FILTER_BILINEAR)
    rl.unload_image(white_img)

    # Cache all uniform locations
    for uniform in self.locations.keys():
      self.locations[uniform] = rl.get_shader_location(self.shader, uniform)

    # Setup default MVP matrix
    mvp_ptr = rl.ffi.new("float[16]", [1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0])
    rl.set_shader_value_matrix(self.shader, self.locations['mvp'], rl.Matrix(*mvp_ptr))

    self.initialized = True

  def cleanup(self):
    if not self.initialized:
      return

    if self.white_texture:
      rl.unload_texture(self.white_texture)
      self.white_texture = None

    if self.shader:
      rl.unload_shader(self.shader)
      self.shader = None

    self.initialized = False


def draw_polygon(rect: rl.Rectangle, points: np.ndarray, color=None, gradient=None):
  """
  Draw a single polygon (converted to batch of 1)

  Args:
      rect: Rectangle defining the drawing area
      points: numpy array of (x,y) points defining the polygon
      color: Solid fill color (rl.Color)
      gradient: Dict with gradient parameters
  """
  polygon_batch = [{'points': points}]

  if color:
    polygon_batch[0]['color'] = color
  if gradient:
    polygon_batch[0]['gradient'] = gradient

  draw_polygons_batch(rect, polygon_batch)


def draw_polygons_batch(rect: rl.Rectangle, polygon_batch):
  """
  Draw multiple polygons in a single draw call

  Args:
      rect: Rectangle defining the drawing area
      polygon_batch: List of dicts with:
          {
              'points': np.ndarray,     # (x,y) points
              'color': rl.Color,        # Solid color (optional)
              'gradient': dict          # Gradient config (optional)
                  {
                      'start': (x1, y1),    # Start point (normalized 0-1)
                      'end': (x2, y2),      # End point (normalized 0-1)
                      'colors': [rl.Color], # List of colors (max 4)
                      'stops': [float]      # List of positions (0-1)
                  }
          }
  """
  if not polygon_batch or len(polygon_batch) == 0:
    return

  state = ShaderState.get_instance()
  if not state.initialized:
    state.initialize()

  # Limit batch size
  batch_size = min(len(polygon_batch), MAX_BATCH_POLYGONS)

  # Reset batch data
  point_offset = 0
  valid_polygons = 0

  # Pack all polygon data
  for i, poly_data in enumerate(polygon_batch[:batch_size]):
    points = poly_data['points']
    if len(points) < 3:
      continue

    # Transform points relative to rect
    transformed_points = points - np.array([rect.x, rect.y])

    # Check if we have room for more points
    if point_offset + len(transformed_points) > MAX_BATCH_POINTS:
      break

    # Store polygon info
    state.polygon_starts_ptr[valid_polygons] = point_offset
    state.polygon_counts_ptr[valid_polygons] = len(transformed_points)

    # Pack points
    flat_points = transformed_points.flatten().astype(np.float32)
    end_offset = point_offset * 2 + len(flat_points)
    state.all_points_ptr[point_offset * 2 : end_offset] = flat_points
    point_offset += len(transformed_points)

    # Handle color/gradient
    gradient = poly_data.get('gradient')
    if gradient:
      state.use_gradient_flags_ptr[valid_polygons] = 1

      # Gradient start/end (normalized 0-1)
      state.batch_gradient_starts_ptr[valid_polygons * 2 : (valid_polygons + 1) * 2] = gradient['start']
      state.batch_gradient_ends_ptr[valid_polygons * 2 : (valid_polygons + 1) * 2] = gradient['end']

      # Gradient colors (up to 4 per gradient)
      colors = gradient['colors'][:4]  # Limit to 4 colors
      stops = gradient.get('stops', [j / max(1, len(colors) - 1) for j in range(len(colors))])

      state.gradient_color_counts_ptr[valid_polygons] = len(colors)

      # Store gradient colors and stops
      for j, (color, stop) in enumerate(zip(colors, stops)):
        color_idx = valid_polygons * 4 + j  # 4 colors per polygon
        if color_idx < MAX_GRADIENT_COLORS:
          base_idx = color_idx * 4
          state.batch_gradient_colors_ptr[base_idx : base_idx + 4] = [
            color.r / 255.0,
            color.g / 255.0,
            color.b / 255.0,
            color.a / 255.0,
          ]
          state.batch_gradient_stops_ptr[color_idx] = stop
    else:
      state.use_gradient_flags_ptr[valid_polygons] = 0
      color = poly_data.get('color', rl.WHITE)
      state.solid_colors_ptr[valid_polygons * 4 : (valid_polygons + 1) * 4] = [
        color.r / 255.0,
        color.g / 255.0,
        color.b / 255.0,
        color.a / 255.0,
      ]

    valid_polygons += 1

  if valid_polygons == 0:
    return

  # Set shader uniforms
  state.polygon_count_ptr[0] = valid_polygons

  rl.set_shader_value(state.shader, state.locations['polygonCount'], state.polygon_count_ptr, UNIFORM_INT)
  rl.set_shader_value_v(state.shader, state.locations['allPoints'], state.all_points_ptr, UNIFORM_VEC2, point_offset)
  rl.set_shader_value_v(
    state.shader, state.locations['polygonStarts'], state.polygon_starts_ptr, UNIFORM_INT, valid_polygons
  )
  rl.set_shader_value_v(
    state.shader, state.locations['polygonCounts'], state.polygon_counts_ptr, UNIFORM_INT, valid_polygons
  )
  rl.set_shader_value_v(
    state.shader, state.locations['solidColors'], state.solid_colors_ptr, UNIFORM_VEC4, valid_polygons
  )
  rl.set_shader_value_v(
    state.shader, state.locations['useGradientFlags'], state.use_gradient_flags_ptr, UNIFORM_INT, valid_polygons
  )
  rl.set_shader_value_v(
    state.shader, state.locations['gradientStarts'], state.batch_gradient_starts_ptr, UNIFORM_VEC2, valid_polygons
  )
  rl.set_shader_value_v(
    state.shader, state.locations['gradientEnds'], state.batch_gradient_ends_ptr, UNIFORM_VEC2, valid_polygons
  )
  rl.set_shader_value_v(
    state.shader,
    state.locations['batchGradientColors'],
    state.batch_gradient_colors_ptr,
    UNIFORM_VEC4,
    valid_polygons * 4,
  )
  rl.set_shader_value_v(
    state.shader,
    state.locations['batchGradientStops'],
    state.batch_gradient_stops_ptr,
    UNIFORM_FLOAT,
    valid_polygons * 4,
  )
  rl.set_shader_value_v(
    state.shader, state.locations['gradientColorCounts'], state.gradient_color_counts_ptr, UNIFORM_INT, valid_polygons
  )

  state.resolution_ptr[0:2] = [rect.width, rect.height]
  rl.set_shader_value(state.shader, state.locations['resolution'], state.resolution_ptr, UNIFORM_VEC2)

  # Render batch
  rl.begin_shader_mode(state.shader)
  rl.draw_texture_pro(
    state.white_texture,
    rl.Rectangle(0, 0, 2, 2),
    rect,
    rl.Vector2(0, 0),
    0.0,
    rl.WHITE,
  )
  rl.end_shader_mode()


def cleanup_shader_resources():
  state = ShaderState.get_instance()
  state.cleanup()


# Example usage functions
def create_road_batch():
  """Example of creating a batch for road rendering"""
  return [
    {
      'points': np.array([[396, 1512], [638, 1160], [768, 966], [1088, 552], [1848, 1512]]),
      'gradient': {
        'start': (0.5, 0.0),
        'end': (0.5, 1.0),
        'colors': [rl.Color(120, 120, 120, 255), rl.Color(80, 80, 80, 255)],
        'stops': [0.0, 1.0],
      },
    },
    {'points': np.array([[100, 100], [200, 100], [150, 200]]), 'color': rl.Color(255, 0, 0, 128)},
  ]
