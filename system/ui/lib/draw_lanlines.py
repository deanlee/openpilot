import pyray as rl
import numpy as np
from typing import Any

MAX_GRADIENT_COLORS = 60  # 4 colors per gradient * 15 polygons
MAX_BATCH_POLYGONS = 8
MAX_BATCH_POINTS = 300

FRAGMENT_SHADER = """
#version 300 es
precision mediump float;

in vec2 fragTexCoord;
out vec4 finalColor;

uniform vec2 allPoints[500];           // Polygon points
uniform int polygonStarts[8];         // Start index per polygon
uniform int polygonCounts[8];         // Point count per polygon
uniform int polygonCount;              // Number of polygons
uniform vec4 solidColors[8];          // Solid colors
uniform int useGradientFlags[8];      // 1 for gradient, 0 for solid
uniform vec2 gradientStarts[8];       // Gradient start
uniform vec2 gradientEnds[8];         // Gradient end
uniform vec4 batchGradientColors[60];  // Up to 15 colors
uniform float batchGradientStops[60];  // Gradient stops
uniform int gradientColorCounts[8];   // Colors per gradient
uniform vec2 resolution;

// Check if point is inside polygon using ray-casting
bool isPointInsidePolygon(vec2 p, int startIdx, int pointCount) {
  if (pointCount < 3) return false;

  int crossings = 0;
  for (int i = 0, j = pointCount - 1; i < pointCount; j = i++) {
    vec2 pi = allPoints[startIdx + i];
    vec2 pj = allPoints[startIdx + j];

    // Skip degenerate edges
    if (distance(pi, pj) < 0.001) continue;

    // Ray-casting
    if (((pi.y > p.y) != (pj.y > p.y)) &&
        (p.x < (pj.x - pi.x) * (p.y - pi.y) / (pj.y - pi.y + 0.001) + pi.x)) {
      crossings++;
    }
  }
  return (crossings & 1) == 1;
}

// Distance to nearest polygon edge
float distanceToEdge(vec2 p, int startIdx, int pointCount) {
  float minDist = 1e10;

  for (int i = 0, j = pointCount - 1; i < pointCount; j = i++) {
    vec2 edge0 = allPoints[startIdx + j];
    vec2 edge1 = allPoints[startIdx + i];

    if (distance(edge0, edge1) < 0.0001) continue;

    vec2 v1 = p - edge0;
    vec2 v2 = edge1 - edge0;
    float l2 = dot(v2, v2);

    if (l2 < 0.0001) {
      float dist = length(v1);
      minDist = min(minDist, dist);
      continue;
    }

    float t = clamp(dot(v1, v2) / l2, 0.0, 1.0);
    vec2 projection = edge0 + t * v2;
    float dist = length(p - projection);
    minDist = min(minDist, dist);
  }

  return minDist;
}

// Signed distance to polygon
float signedDistanceToPolygon(vec2 p, int polyIndex) {
  int startIdx = polygonStarts[polyIndex];
  int pointCount = polygonCounts[polyIndex];
  float dist = distanceToEdge(p, startIdx, pointCount);
  bool inside = isPointInsidePolygon(p, startIdx, pointCount);
  return inside ? dist : -dist;
}

// Qt-like linear gradient
vec4 getBatchGradientColor(vec2 pos, int polyIndex) {
  // return batchGradientColors[1]; // Default fallback color
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
  //return batchGradientColors[colorStart + colorCount - 1];
  // Debug: Tint green if fallback to last color
  return batchGradientColors[colorStart + colorCount - 1] * vec4(0.0, 1.0, 0.0, 1.0);
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
    // Compute signed distance
    float sd = signedDistanceToPolygon(pixel, i);

    // Check if pixel is inside or near edge
    if (sd >= -aaWidth) { // Inside or within anti-aliasing range
      // Anti-aliasing
      float alpha = sd > aaWidth ? 1.0 : smoothstep(-aaWidth, aaWidth, sd);
      if (alpha <= 0.0) continue;

      // Apply color
      vec4 color = getColor(i);
      finalResult = vec4(color.rgb, color.a *alpha);
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

    # Cache for batch state
    self.last_batch_id = None
    self.last_rect = None

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

def clip_to_screen(bounding_rect: rl.Rectangle, screen_width: float, screen_height: float) -> rl.Rectangle:
  """Clip the bounding rectangle to screen boundaries."""
  min_x = max(bounding_rect.x, 0.0)
  max_x = min(bounding_rect.x + bounding_rect.width, screen_width)
  min_y = max(bounding_rect.y, 0.0)
  max_y = min(bounding_rect.y + bounding_rect.height, screen_height)

  width = max(max_x - min_x, 1.0)
  height = max(max_y - min_y, 1.0)

  if width <= 1.0 or height <= 1.0:
    return None  # Invisible

  return rl.Rectangle(min_x, min_y, width, height)

def compute_bounding_rect(polygon_batch, original_rect: rl.Rectangle) -> rl.Rectangle:
  """Compute the AABB of all polygons, constrained to original_rect."""
  if not polygon_batch:
    return original_rect

  min_x, min_y = np.inf, np.inf
  max_x, max_y = -np.inf, -np.inf

  for poly_data in polygon_batch:
    points = poly_data.get('points')
    if points is None or len(points) < 3:
      continue
    min_x = min(min_x, np.min(points[:, 0]))
    max_x = max(max_x, np.max(points[:, 0]))
    min_y = min(min_y, np.min(points[:, 1]))
    max_y = max(max_y, np.max(points[:, 1]))

  if min_x == np.inf:
    return original_rect

  # Constrain to original_rect
  min_x = max(min_x, original_rect.x)
  max_x = min(max_x, original_rect.x + original_rect.width)
  min_y = max(min_y, original_rect.y)
  max_y = min(max_y, original_rect.y + original_rect.height)

  width = max(max_x - min_x, 1.0)
  height = max(max_y - min_y, 1.0)

  return rl.Rectangle(min_x, min_y, width, height)

def _update_batch_state(state, polygon_batch, clipped_rect: rl.Rectangle, original_rect: rl.Rectangle):
  batch_size = min(len(polygon_batch), MAX_BATCH_POLYGONS)
  point_offset = 0
  valid_polygons = 0

  for i, poly_data in enumerate(polygon_batch[:batch_size]):
    points = poly_data.get('points')
    if points is None or len(points) < 3:
      continue

    # Transform points to clipped_rect's origin
    transformed_points = points - np.array([clipped_rect.x, clipped_rect.y])

    # Cull points outside clipped_rect (optional, for simplicity skip full clipping)
    if point_offset + len(transformed_points) > MAX_BATCH_POINTS:
      break

    state.polygon_starts_ptr[valid_polygons] = point_offset
    state.polygon_counts_ptr[valid_polygons] = len(transformed_points)

    flat_points = transformed_points.flatten().astype(np.float32)
    end_offset = point_offset * 2 + len(flat_points)
    state.all_points_ptr[point_offset * 2 : end_offset] = flat_points
    point_offset += len(transformed_points)

    gradient = poly_data.get('gradient')
    if gradient:
      state.use_gradient_flags_ptr[valid_polygons] = 1

      # Transform gradient to clipped_rect coordinates
      start = np.array(gradient['start']) * np.array([original_rect.width, original_rect.height]) + np.array([original_rect.x, original_rect.y])
      end = np.array(gradient['end']) * np.array([original_rect.width, original_rect.height]) + np.array([original_rect.x, original_rect.y])
      start = start - np.array([clipped_rect.x, clipped_rect.y])
      end = end - np.array([clipped_rect.x, clipped_rect.y])

      state.batch_gradient_starts_ptr[valid_polygons * 2 : (valid_polygons + 1) * 2] = start.astype(np.float32)
      state.batch_gradient_ends_ptr[valid_polygons * 2 : (valid_polygons + 1) * 2] = end.astype(np.float32)

      colors = gradient['colors'][:4]
      stops = gradient.get('stops', [j / max(1, len(colors) - 1) for j in range(len(colors))])

      stops = np.array(stops, dtype=np.float32)
      if len(stops) > 1:
        stops = np.clip(stops, 0.0, 1.0)
        stops = np.sort(stops)
        if np.all(stops == stops[0]):
          stops = np.linspace(0.0, 1.0, len(stops))

      print(f"Polygon {i}: colors={len(colors)}, stops={stops.tolist()}, start={start}, end={end}")

      state.gradient_color_counts_ptr[valid_polygons] = len(colors)

      for j, (color, stop) in enumerate(zip(colors, stops)):
        color_idx = valid_polygons * 4 + j
        if color_idx < MAX_GRADIENT_COLORS:
          base_idx = color_idx * 4
          state.batch_gradient_colors_ptr[base_idx : base_idx + 4] = [
            color.r / 255.0,
            color.g / 255.0,
            color.b / 255.0,
            color.a / 255.0,
          ]
          state.batch_gradient_stops_ptr[color_idx] = float(stop)
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

  state.polygon_count_ptr[0] = valid_polygons
  rl.set_shader_value(state.shader, state.locations['polygonCount'], state.polygon_count_ptr, UNIFORM_INT)

  rl.set_shader_value_v(state.shader, state.locations['allPoints'], state.all_points_ptr, UNIFORM_VEC2, point_offset)
  rl.set_shader_value_v(state.shader, state.locations['polygonStarts'], state.polygon_starts_ptr, UNIFORM_INT, valid_polygons)
  rl.set_shader_value_v(state.shader, state.locations['polygonCounts'], state.polygon_counts_ptr, UNIFORM_INT, valid_polygons)
  rl.set_shader_value_v(state.shader, state.locations['solidColors'], state.solid_colors_ptr, UNIFORM_VEC4, valid_polygons)
  rl.set_shader_value_v(state.shader, state.locations['useGradientFlags'], state.use_gradient_flags_ptr, UNIFORM_INT, valid_polygons)
  rl.set_shader_value_v(state.shader, state.locations['gradientStarts'], state.batch_gradient_starts_ptr, UNIFORM_VEC2, valid_polygons)
  rl.set_shader_value_v(state.shader, state.locations['gradientEnds'], state.batch_gradient_ends_ptr, UNIFORM_VEC2, valid_polygons)
  rl.set_shader_value_v(state.shader, state.locations['batchGradientColors'], state.batch_gradient_colors_ptr, UNIFORM_VEC4, valid_polygons * 4)
  rl.set_shader_value_v(state.shader, state.locations['batchGradientStops'], state.batch_gradient_stops_ptr, UNIFORM_FLOAT, valid_polygons * 4)
  rl.set_shader_value_v(state.shader, state.locations['gradientColorCounts'], state.gradient_color_counts_ptr, UNIFORM_INT, valid_polygons)

  state.resolution_ptr[0:2] = [clipped_rect.width, clipped_rect.height]
  rl.set_shader_value(state.shader, state.locations['resolution'], state.resolution_ptr, UNIFORM_VEC2)

def draw_polygon(rect: rl.Rectangle, points: np.ndarray, color=None, gradient=None):
  polygon_batch = [{'points': points}]
  if color:
    polygon_batch[0]['color'] = color
  if gradient:
    polygon_batch[0]['gradient'] = gradient
  draw_polygons_batch(rect, polygon_batch)

def draw_polygons_batch(original_rect: rl.Rectangle, polygon_batch):
  if not polygon_batch or len(polygon_batch) == 0:
    return

  state = ShaderState.get_instance()
  if not state.initialized:
    state.initialize()

  # Compute bounding rectangle
  bounding_rect = compute_bounding_rect(polygon_batch, original_rect)

  # Clip to screen
  screen_width = original_rect.width  # Assume original_rect is screen
  screen_height = original_rect.height
  clipped_rect = clip_to_screen(bounding_rect, screen_width, screen_height)

  if clipped_rect is None:
    print("Polygon batch entirely off-screen")
    return

  current_batch_id = id(polygon_batch)
  current_rect = (clipped_rect.x, clipped_rect.y, clipped_rect.width, clipped_rect.height)
  same_batch = state.last_batch_id == current_batch_id and state.last_rect_dims == current_rect
  if not same_batch:
    _update_batch_state(state, polygon_batch, clipped_rect, original_rect)
    state.last_batch_id = current_batch_id
    state.last_rect_dims = current_rect
  else:
    print('Using cached state')

  rl.begin_shader_mode(state.shader)
  rl.draw_texture_pro(state.white_texture, rl.Rectangle(0, 0, 2, 2), clipped_rect, rl.Vector2(0, 0), 0.0, rl.WHITE)
  rl.end_shader_mode()

def cleanup_shader_resources():
  state = ShaderState.get_instance()
  state.cleanup()
