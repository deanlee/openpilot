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

// Single polygon uniforms (legacy)
uniform vec2 points[100];
uniform int pointCount;
uniform vec4 fillColor;
uniform bool useGradient;
uniform vec2 gradientStart;
uniform vec2 gradientEnd;
uniform vec4 gradientColors[15];
uniform float gradientStops[15];
uniform int gradientColorCount;
uniform vec2 visibleGradientRange;

// Batch polygon data
uniform vec2 allPoints[300];           // All polygon points in one array
uniform int polygonStarts[15];         // Start index for each polygon
uniform int polygonCounts[15];         // Point count for each polygon
uniform int polygonCount;              // Total number of polygons (0 for single mode)

// Color data for each polygon
uniform vec4 solidColors[15];          // Solid colors for each polygon
uniform int useGradientFlags[15];      // 1 if polygon uses gradient, 0 for solid

// Gradient data (4 colors max per polygon)
uniform vec2 gradientStarts[15];
uniform vec2 gradientEnds[15];
uniform vec4 batchGradientColors[60];  // 4 colors per gradient max (15*4)
uniform float batchGradientStops[60];
uniform int gradientColorCounts[15];   // Colors per gradient

uniform vec2 resolution;

// Single polygon gradient function
vec4 getSingleGradientColor(vec2 pos) {
  vec2 gradientDir = gradientEnd - gradientStart;
  float gradientLength = length(gradientDir);
  if (gradientLength < 0.001) return gradientColors[0];

  vec2 normalizedDir = gradientDir / gradientLength;
  vec2 pointVec = pos - gradientStart;
  float projection = dot(pointVec, normalizedDir);

  float t = projection / gradientLength;

  // Gradient clipping: remap t to visible range
  float visibleStart = visibleGradientRange.x;
  float visibleEnd = visibleGradientRange.y;
  float visibleRange = visibleEnd - visibleStart;

  if (visibleRange > 0.001) {
    t = visibleStart + t * visibleRange;
  }

  t = clamp(t, 0.0, 1.0);

  for (int i = 0; i < gradientColorCount - 1; i++) {
    if (t >= gradientStops[i] && t <= gradientStops[i+1]) {
      float segmentT = (t - gradientStops[i]) / (gradientStops[i+1] - gradientStops[i]);
      return mix(gradientColors[i], gradientColors[i+1], segmentT);
    }
  }

  return gradientColors[gradientColorCount-1];
}

// Batch polygon gradient function
vec4 getBatchGradientColor(vec2 pos, int polyIndex) {
  int colorStart = polyIndex * 4;  // Each gradient can have up to 4 colors
  int colorCount = gradientColorCounts[polyIndex];

  vec2 gradientDir = gradientEnds[polyIndex] - gradientStarts[polyIndex];
  float gradientLength = length(gradientDir);
  if (gradientLength < 0.001) return batchGradientColors[colorStart];

  vec2 normalizedDir = gradientDir / gradientLength;
  vec2 pointVec = pos - gradientStarts[polyIndex];
  float projection = dot(pointVec, normalizedDir);

  float t = clamp(projection / gradientLength, 0.0, 1.0);

  // Find color segment
  for (int i = 0; i < colorCount - 1; i++) {
    int stopIdx = colorStart + i;
    if (t >= batchGradientStops[stopIdx] && t <= batchGradientStops[stopIdx + 1]) {
      float segmentT = (t - batchGradientStops[stopIdx]) / (batchGradientStops[stopIdx + 1] - batchGradientStops[stopIdx]);
      return mix(batchGradientColors[stopIdx], batchGradientColors[stopIdx + 1], segmentT);
    }
  }

  return batchGradientColors[colorStart + colorCount - 1];
}

// Single polygon inside test
bool isPointInsideSinglePolygon(vec2 p) {
  if (pointCount < 3) return false;

  int crossings = 0;
  for (int i = 0, j = pointCount - 1; i < pointCount; j = i++) {
    vec2 pi = points[i];
    vec2 pj = points[j];

    if (distance(pi, pj) < 0.001) continue;

    if (((pi.y > p.y) != (pj.y > p.y)) &&
        (p.x < (pj.x - pi.x) * (p.y - pi.y) / (pj.y - pi.y + 0.001) + pi.x)) {
      crossings++;
    }
  }
  return (crossings & 1) == 1;
}

// Batch polygon inside test
bool isPointInsidePolygon(vec2 p, int polyIndex) {
  int startIdx = polygonStarts[polyIndex];
  int pointCount = polygonCounts[polyIndex];

  if (pointCount < 3) return false;

  int crossings = 0;
  for (int i = 0, j = pointCount - 1; i < pointCount; j = i++) {
    vec2 pi = allPoints[startIdx + i];
    vec2 pj = allPoints[startIdx + j];

    if (distance(pi, pj) < 0.001) continue;

    if (((pi.y > p.y) != (pj.y > p.y)) &&
        (p.x < (pj.x - pi.x) * (p.y - pi.y) / (pj.y - pi.y + 0.001) + pi.x)) {
      crossings++;
    }
  }
  return (crossings & 1) == 1;
}

// Single polygon distance
float distanceToSinglePolygonEdge(vec2 p) {
  float minDist = 1000.0;

  for (int i = 0, j = pointCount - 1; i < pointCount; j = i++) {
    vec2 edge0 = points[j];
    vec2 edge1 = points[i];

    if (distance(edge0, edge1) < 0.0001) continue;

    vec2 v1 = p - edge0;
    vec2 v2 = edge1 - edge0;
    float l2 = dot(v2, v2);

    if (l2 < 0.0001) {
      minDist = min(minDist, length(v1));
      continue;
    }

    float t = clamp(dot(v1, v2) / l2, 0.0, 1.0);
    vec2 projection = edge0 + t * v2;
    minDist = min(minDist, length(p - projection));
  }

  return minDist;
}

// Batch polygon distance
float distanceToPolygonEdge(vec2 p, int polyIndex) {
  int startIdx = polygonStarts[polyIndex];
  int pointCount = polygonCounts[polyIndex];
  float minDist = 1000.0;

  for (int i = 0, j = pointCount - 1; i < pointCount; j = i++) {
    vec2 edge0 = allPoints[startIdx + j];
    vec2 edge1 = allPoints[startIdx + i];

    if (distance(edge0, edge1) < 0.0001) continue;

    vec2 v1 = p - edge0;
    vec2 v2 = edge1 - edge0;
    float l2 = dot(v2, v2);

    if (l2 < 0.0001) {
      minDist = min(minDist, length(v1));
      continue;
    }

    float t = clamp(dot(v1, v2) / l2, 0.0, 1.0);
    vec2 projection = edge0 + t * v2;
    minDist = min(minDist, length(p - projection));
  }

  return minDist;
}

void main() {
  vec2 pixel = fragTexCoord * resolution;
  vec4 finalResult = vec4(0.0);

  float aaWidth = 0.5;
  if (polygonCount > 0) {
    // Batch mode: Process multiple polygons
    for (int polyIndex = 0; polyIndex < polygonCount; polyIndex++) {
      bool inside = isPointInsidePolygon(pixel, polyIndex);
      if (!inside) continue;

      float dist = distanceToPolygonEdge(pixel, polyIndex);

      float alpha = smoothstep(-aaWidth, aaWidth, dist);
      if (alpha <= 0.0) continue;

      // Get color for this polygon
      vec4 color;
      if (useGradientFlags[polyIndex] == 1) {
        color = getBatchGradientColor(fragTexCoord, polyIndex);
      } else {
        color = solidColors[polyIndex];
      }

      // Apply alpha and blend
      color.a *= alpha;

      // Alpha blending: src_over operation
      finalResult.rgb = finalResult.rgb * (1.0 - color.a) + color.rgb * color.a;
      finalResult.a = finalResult.a + color.a * (1.0 - finalResult.a);
    }
  } else {
    // Single polygon mode (legacy)
    bool inside = isPointInsideSinglePolygon(pixel);
    if (inside) {
      float dist = distanceToSinglePolygonEdge(pixel);

      vec2 pixelGrad = vec2(dFdx(pixel.x), dFdy(pixel.y));
      float pixelSize = length(pixelGrad);
      float aaWidth = max(0.5, pixelSize * 0.5);

      float alpha = smoothstep(-aaWidth, aaWidth, dist);
      if (alpha > 0.0) {
        vec4 color = useGradient ? getSingleGradientColor(fragTexCoord) : fillColor;
        finalResult = vec4(color.rgb, color.a * alpha);
      }
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
      # Single polygon uniforms
      'pointCount': None,
      'fillColor': None,
      'resolution': None,
      'points': None,
      'useGradient': None,
      'gradientStart': None,
      'gradientEnd': None,
      'gradientColors': None,
      'gradientStops': None,
      'gradientColorCount': None,
      'mvp': None,
      'visibleGradientRange': None,
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

    # Single polygon FFI objects
    self.point_count_ptr = rl.ffi.new("int[]", [0])
    self.resolution_ptr = rl.ffi.new("float[]", [0.0, 0.0])
    self.fill_color_ptr = rl.ffi.new("float[]", [0.0, 0.0, 0.0, 0.0])
    self.use_gradient_ptr = rl.ffi.new("int[]", [0])
    self.gradient_start_ptr = rl.ffi.new("float[]", [0.0, 0.0])
    self.gradient_end_ptr = rl.ffi.new("float[]", [0.0, 0.0])
    self.color_count_ptr = rl.ffi.new("int[]", [0])
    self.visible_gradient_range_ptr = rl.ffi.new("float[]", [0.0, 0.0])
    self.gradient_colors_ptr = rl.ffi.new("float[]", 15 * 4)  # Single mode max 15 colors
    self.gradient_stops_ptr = rl.ffi.new("float[]", 15)

    # Batch FFI objects
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


def _configure_shader_color(state, color, gradient, rect, min_xy, max_xy):
  """Configure shader uniforms for solid color or gradient rendering"""
  use_gradient = 1 if gradient else 0
  state.use_gradient_ptr[0] = use_gradient
  rl.set_shader_value(state.shader, state.locations['useGradient'], state.use_gradient_ptr, UNIFORM_INT)

  if use_gradient:
    # Set gradient start/end
    state.gradient_start_ptr[0:2] = gradient['start']
    state.gradient_end_ptr[0:2] = gradient['end']
    rl.set_shader_value(state.shader, state.locations['gradientStart'], state.gradient_start_ptr, UNIFORM_VEC2)
    rl.set_shader_value(state.shader, state.locations['gradientEnd'], state.gradient_end_ptr, UNIFORM_VEC2)

    # Calculate visible gradient range
    width = max_xy[0] - min_xy[0]
    height = max_xy[1] - min_xy[1]

    gradient_dir = (gradient['end'][0] - gradient['start'][0], gradient['end'][1] - gradient['start'][1])
    is_vertical = abs(gradient_dir[1]) > abs(gradient_dir[0])

    visible_start = 0.0
    visible_end = 1.0

    if is_vertical and height > 0:
      visible_start = (rect.y - min_xy[1]) / height
      visible_end = visible_start + rect.height / height
    elif width > 0:
      visible_start = (rect.x - min_xy[0]) / width
      visible_end = visible_start + rect.width / width

    # Clamp visible range
    visible_start = max(0.0, min(1.0, visible_start))
    visible_end = max(0.0, min(1.0, visible_end))

    state.visible_gradient_range_ptr[0:2] = [visible_start, visible_end]
    rl.set_shader_value(state.shader, state.locations['visibleGradientRange'], state.visible_gradient_range_ptr, UNIFORM_VEC2)

    # Set gradient colors
    colors = gradient['colors']
    color_count = min(len(colors), 15)  # Single mode max 15 colors
    for i, c in enumerate(colors[:color_count]):
      base_idx = i * 4
      state.gradient_colors_ptr[base_idx:base_idx+4] = [c.r / 255.0, c.g / 255.0, c.b / 255.0, c.a / 255.0]
    rl.set_shader_value_v(state.shader, state.locations['gradientColors'], state.gradient_colors_ptr, UNIFORM_VEC4, color_count)

    # Set gradient stops
    stops = gradient.get('stops', [i / max(1, color_count - 1) for i in range(color_count)])
    state.gradient_stops_ptr[0:color_count] = stops[:color_count]
    rl.set_shader_value_v(state.shader, state.locations['gradientStops'], state.gradient_stops_ptr, UNIFORM_FLOAT, color_count)

    # Set color count
    state.color_count_ptr[0] = color_count
    rl.set_shader_value(state.shader, state.locations['gradientColorCount'], state.color_count_ptr, UNIFORM_INT)
  else:
    color = color or rl.WHITE  # Default to white if no color provided
    state.fill_color_ptr[0:4] = [color.r / 255.0, color.g / 255.0, color.b / 255.0, color.a / 255.0]
    rl.set_shader_value(state.shader, state.locations['fillColor'], state.fill_color_ptr, UNIFORM_VEC4)


def draw_polygon(rect: rl.Rectangle, points: np.ndarray, color=None, gradient=None):
  """
  Draw a complex polygon using shader-based even-odd fill rule

  Args:
      rect: Rectangle defining the drawing area
      points: numpy array of (x,y) points defining the polygon
      color: Solid fill color (rl.Color)
      gradient: Dict with gradient parameters:
          {
              'start': (x1, y1),    # Start point (normalized 0-1)
              'end': (x2, y2),      # End point (normalized 0-1)
              'colors': [rl.Color], # List of colors at stops
              'stops': [float]      # List of positions (0-1)
          }
  """
  if len(points) < 3:
    return

  state = ShaderState.get_instance()
  if not state.initialized:
    state.initialize()

  # Set polygon count to 0 for single polygon mode
  state.polygon_count_ptr[0] = 0
  rl.set_shader_value(state.shader, state.locations['polygonCount'], state.polygon_count_ptr, UNIFORM_INT)

  # Find bounding box
  min_xy = np.min(points, axis=0)
  max_xy = np.max(points, axis=0)

  # Clip coordinates to rectangle
  clip_x = max(rect.x, min_xy[0])
  clip_y = max(rect.y, min_xy[1])
  clip_right = min(rect.x + rect.width, max_xy[0])
  clip_bottom = min(rect.y + rect.height, max_xy[1])

  # Check if polygon is completely off-screen
  if clip_x >= clip_right or clip_y >= clip_bottom:
    return

  clipped_width = clip_right - clip_x
  clipped_height = clip_bottom - clip_y

  clip_rect = rl.Rectangle(clip_x, clip_y, clipped_width, clipped_height)

  # Transform points relative to the CLIPPED area
  transformed_points = points - np.array([clip_x, clip_y])

  # Set shader values
  state.point_count_ptr[0] = len(transformed_points)
  rl.set_shader_value(state.shader, state.locations['pointCount'], state.point_count_ptr, UNIFORM_INT)

  state.resolution_ptr[0:2] = [clipped_width, clipped_height]
  rl.set_shader_value(state.shader, state.locations['resolution'], state.resolution_ptr, UNIFORM_VEC2)

  flat_points = np.ascontiguousarray(transformed_points.flatten().astype(np.float32))
  points_ptr = rl.ffi.cast("float *", flat_points.ctypes.data)
  rl.set_shader_value_v(state.shader, state.locations['points'], points_ptr, UNIFORM_VEC2, len(transformed_points))

  _configure_shader_color(state, color, gradient, clip_rect, min_xy, max_xy)

  # Render
  rl.begin_shader_mode(state.shader)
  rl.draw_texture_pro(
    state.white_texture,
    rl.Rectangle(0, 0, 2, 2),
    clip_rect,
    rl.Vector2(0, 0),
    0.0,
    rl.WHITE,
  )
  rl.end_shader_mode()


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
    state.all_points_ptr[point_offset * 2:end_offset] = flat_points
    point_offset += len(transformed_points)

    # Handle color/gradient
    gradient = poly_data.get('gradient')
    if gradient:
      state.use_gradient_flags_ptr[valid_polygons] = 1

      # Gradient start/end (normalized 0-1)
      state.batch_gradient_starts_ptr[valid_polygons * 2:(valid_polygons + 1) * 2] = gradient['start']
      state.batch_gradient_ends_ptr[valid_polygons * 2:(valid_polygons + 1) * 2] = gradient['end']

      # Gradient colors (up to 4 per gradient)
      colors = gradient['colors'][:4]  # Limit to 4 colors
      stops = gradient.get('stops', [j / max(1, len(colors) - 1) for j in range(len(colors))])

      state.gradient_color_counts_ptr[valid_polygons] = len(colors)

      # Store gradient colors and stops
      for j, (color, stop) in enumerate(zip(colors, stops)):
        color_idx = valid_polygons * 4 + j  # 4 colors per polygon
        if color_idx < MAX_GRADIENT_COLORS:
          base_idx = color_idx * 4
          state.batch_gradient_colors_ptr[base_idx:base_idx + 4] = [
            color.r / 255.0, color.g / 255.0, color.b / 255.0, color.a / 255.0
          ]
          state.batch_gradient_stops_ptr[color_idx] = stop
    else:
      state.use_gradient_flags_ptr[valid_polygons] = 0
      color = poly_data.get('color', rl.WHITE)
      state.solid_colors_ptr[valid_polygons * 4:(valid_polygons + 1) * 4] = [
        color.r / 255.0, color.g / 255.0, color.b / 255.0, color.a / 255.0
      ]

    valid_polygons += 1

  if valid_polygons == 0:
    return

  # Set shader uniforms
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
        'stops': [0.0, 1.0]
      }
    },
    {
      'points': np.array([[100, 100], [200, 100], [150, 200]]),
      'color': rl.Color(255, 0, 0, 128)
    }
  ]