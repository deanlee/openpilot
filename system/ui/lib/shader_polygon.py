import pyray as rl
import numpy as np
from typing import Any

MAX_GRADIENT_COLORS = 5  # Reduced to 5 colors per gradient
MAX_POLYGONS = 7  # Support up to 7 polygons

FRAGMENT_SHADER = """
#version 300 es
precision mediump float;

in vec2 fragTexCoord;
out vec4 finalColor;

// Points for all polygons
uniform vec2 points[200];

// Number of points in each polygon and starting indices
uniform int pointCounts[7];
uniform int pointStarts[7];
uniform int polygonCount;

// Color info for each polygon
uniform vec4 fillColors[7];
uniform bool useGradient[7];
uniform vec2 gradientStart[7];
uniform vec2 gradientEnd[7];

// All gradient colors for all polygons
uniform vec4 gradientColors[35]; // 7 polygons * 5 colors each
uniform float gradientStops[35]; // 7 polygons * 5 stops each
uniform int gradientColorCounts[7];
uniform int gradientColorStarts[7];

uniform vec2 resolution;
uniform vec2 visibleGradientRange[7];

vec4 getGradientColor(vec2 pos, int polygonIndex) {
  vec2 gradStart = gradientStart[polygonIndex];
  vec2 gradEnd = gradientEnd[polygonIndex];
  vec2 gradientDir = gradEnd - gradStart;
  float gradientLength = length(gradientDir);

  int colorStart = gradientColorStarts[polygonIndex];
  int colorCount = gradientColorCounts[polygonIndex];

  if (gradientLength < 0.001) return gradientColors[colorStart];

  vec2 normalizedDir = gradientDir / gradientLength;
  vec2 pointVec = pos - gradStart;
  float projection = dot(pointVec, normalizedDir);

  float t = projection / gradientLength;

  // Gradient clipping: remap t to visible range
  float visibleStart = visibleGradientRange[polygonIndex].x;
  float visibleEnd = visibleGradientRange[polygonIndex].y;
  float visibleRange = visibleEnd - visibleStart;

  // Remap t to visible range
  if (visibleRange > 0.001) {
    t = visibleStart + t * visibleRange;
  }

  t = clamp(t, 0.0, 1.0);
  for (int i = 0; i < colorCount - 1; i++) {
    if (t >= gradientStops[colorStart + i] && t <= gradientStops[colorStart + i + 1]) {
      float segmentT = (t - gradientStops[colorStart + i]) /
                      (gradientStops[colorStart + i + 1] - gradientStops[colorStart + i]);
      return mix(gradientColors[colorStart + i], gradientColors[colorStart + i + 1], segmentT);
    }
  }

  return gradientColors[colorStart + colorCount - 1];
}

bool isPointInsidePolygon(vec2 p, int startIdx, int count) {
  if (count < 3) return false;

  int crossings = 0;
  for (int i = 0, j = count - 1; i < count; j = i++) {
    vec2 pi = points[startIdx + i];
    vec2 pj = points[startIdx + j];

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

float distanceToEdge(vec2 p, int startIdx, int count) {
  float minDist = 1000.0;

  for (int i = 0, j = count - 1; i < count; j = i++) {
    vec2 edge0 = points[startIdx + j];
    vec2 edge1 = points[startIdx + i];

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

float signedDistanceToPolygon(vec2 p, int startIdx, int count) {
  float dist = distanceToEdge(p, startIdx, count);
  bool inside = isPointInsidePolygon(p, startIdx, count);
  return inside ? dist : -dist;
}

void main() {
  vec2 pixel = fragTexCoord * resolution;
  vec4 result = vec4(0.0);

  // Process each polygon from back to front for proper alpha blending
  for (int polyIdx = 0; polyIdx < polygonCount; polyIdx++) {
    int startIdx = pointStarts[polyIdx];
    int count = pointCounts[polyIdx];

    float signedDist = signedDistanceToPolygon(pixel, startIdx, count);

    vec2 pixelGrad = vec2(dFdx(pixel.x), dFdy(pixel.y));
    float pixelSize = length(pixelGrad);
    float aaWidth = max(0.5, pixelSize * 0.5); // Sharper anti-aliasing

    float alpha = smoothstep(-aaWidth, aaWidth, signedDist);

    if (alpha > 0.0) {
      vec4 color = useGradient[polyIdx] ?
                  getGradientColor(fragTexCoord, polyIdx) :
                  fillColors[polyIdx];

      // Premultiplied alpha blending
      color.rgb *= color.a * alpha;
      color.a *= alpha;

      // Blend with previous result
      result = result * (1.0 - color.a) + color;
    }
  }

  finalColor = result;
}
"""

# Same vertex shader as before
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
      'points': None,
      'pointCounts': None,
      'pointStarts': None,
      'polygonCount': None,
      'fillColors': None,
      'useGradient': None,
      'gradientStart': None,
      'gradientEnd': None,
      'gradientColors': None,
      'gradientStops': None,
      'gradientColorCounts': None,
      'gradientColorStarts': None,
      'resolution': None,
      'mvp': None,
      'visibleGradientRange': None,
    }

    # Pre-allocated FFI objects
    self.polygon_count_ptr = rl.ffi.new("int[]", [0])
    self.point_counts_ptr = rl.ffi.new("int[]", MAX_POLYGONS)
    self.point_starts_ptr = rl.ffi.new("int[]", MAX_POLYGONS)
    self.resolution_ptr = rl.ffi.new("float[]", [0.0, 0.0])
    self.fill_colors_ptr = rl.ffi.new("float[]", MAX_POLYGONS * 4)
    self.use_gradient_ptr = rl.ffi.new("int[]", MAX_POLYGONS)
    self.gradient_start_ptr = rl.ffi.new("float[]", MAX_POLYGONS * 2)
    self.gradient_end_ptr = rl.ffi.new("float[]", MAX_POLYGONS * 2)
    self.color_counts_ptr = rl.ffi.new("int[]", MAX_POLYGONS)
    self.color_starts_ptr = rl.ffi.new("int[]", MAX_POLYGONS)
    self.visible_gradient_range_ptr = rl.ffi.new("float[]", MAX_POLYGONS * 2)
    self.gradient_colors_ptr = rl.ffi.new("float[]", MAX_POLYGONS * MAX_GRADIENT_COLORS * 4)
    self.gradient_stops_ptr = rl.ffi.new("float[]", MAX_POLYGONS * MAX_GRADIENT_COLORS)

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


def draw_polygons(rect: rl.Rectangle, polygons_data):
  """
  Draw multiple polygons in a single shader call

  Args:
      rect: Rectangle defining the drawing area
      polygons_data: List of polygon data, where each item is:
          {
              'points': np.ndarray,   # Array of (x,y) points
              'color': rl.Color,      # Solid fill color (use this for non-gradient fills)
              'gradient': {           # Gradient config (optional, omit for solid color)
                  'start': (x1, y1),  # Start point (normalized 0-1)
                  'end': (x2, y2),    # End point (normalized 0-1)
                  'colors': [rl.Color], # List of colors at stops
                  'stops': [float]    # List of positions (0-1)
              }
          }
  """
  if not polygons_data:
    return

  # Limit to MAX_POLYGONS
  polygons_data = polygons_data[:MAX_POLYGONS]
  polygon_count = len(polygons_data)

  # Filter out polygons with fewer than 3 points
  valid_polygons = []
  for poly in polygons_data:
    if len(poly.get('points', [])) >= 3:
      valid_polygons.append(poly)

  if not valid_polygons:
    return

  polygon_count = len(valid_polygons)

  state = ShaderState.get_instance()
  if not state.initialized:
    state.initialize()

  # Find overall bounding box for all polygons
  all_points = np.concatenate([poly['points'] for poly in valid_polygons])
  min_xy = np.min(all_points, axis=0)
  max_xy = np.max(all_points, axis=0)

  # Clip coordinates to rectangle
  clip_x = max(rect.x, min_xy[0])
  clip_y = max(rect.y, min_xy[1])
  clip_right = min(rect.x + rect.width, max_xy[0])
  clip_bottom = min(rect.y + rect.height, max_xy[1])

  # Check if all polygons are completely off-screen
  if clip_x >= clip_right or clip_y >= clip_bottom:
    return

  clipped_width = clip_right - clip_x
  clipped_height = clip_bottom - clip_y

  clip_rect = rl.Rectangle(clip_x, clip_y, clipped_width, clipped_height)

  # Set up points and their counts/indices
  all_transformed_points = []
  point_start = 0
  color_start = 0

  # Track if any polygons use gradients
  has_gradients = False

  for i, poly in enumerate(valid_polygons):
    points = poly['points']
    # Transform points relative to the clipped area
    transformed_points = points - np.array([clip_x, clip_y])
    all_transformed_points.append(transformed_points)

    # Set point count and start index
    state.point_counts_ptr[i] = len(transformed_points)
    state.point_starts_ptr[i] = point_start
    point_start += len(transformed_points)

    # Set fill color
    color = poly.get('color', rl.WHITE)
    base_idx = i * 4

    # Handle both rl.Color objects and tuples/lists
    if hasattr(color, 'r'):
      # It's an rl.Color object
      state.fill_colors_ptr[base_idx : base_idx + 4] = [
        color.r / 255.0,
        color.g / 255.0,
        color.b / 255.0,
        color.a / 255.0,
      ]
    else:
      # Assume it's a tuple or list of (r, g, b) or (r, g, b, a)
      r, g, b, *a = color + (255,) if len(color) == 3 else color
      state.fill_colors_ptr[base_idx : base_idx + 4] = [
        r / 255.0,
        g / 255.0,
        b / 255.0,
        a[0] / 255.0 if a else 1.0,
      ]

   # Set gradient info
    gradient = poly.get('gradient')
    use_gradient = gradient is not None
    state.use_gradient_ptr[i] = 1 if use_gradient else 0

    if use_gradient:
      has_gradients = True

      # Get the specific polygon's bounding box
      poly_points = poly['points']
      poly_min = np.min(poly_points, axis=0)
      poly_max = np.max(poly_points, axis=0)
      poly_width = poly_max[0] - poly_min[0]
      poly_height = poly_max[1] - poly_min[1]

      # Normalize gradient coordinates to the polygon's bounds
      # This is critical for proper gradient positioning
      start = gradient.get('start', (0.0, 0.0))
      end = gradient.get('end', (0.0, 1.0))

      # Map normalized (0-1) gradient coordinates to actual screen coordinates
      # Then transform to be relative to the clipped area
      start_x = poly_min[0] + start[0] * poly_width - clip_x
      start_y = poly_min[1] + start[1] * poly_height - clip_y
      end_x = poly_min[0] + end[0] * poly_width - clip_x
      end_y = poly_min[1] + end[1] * poly_height - clip_y

      # Set transformed gradient start/end points
      base_idx = i * 2
      state.gradient_start_ptr[base_idx] = start_x / clipped_width
      state.gradient_start_ptr[base_idx + 1] = start_y / clipped_height
      state.gradient_end_ptr[base_idx] = end_x / clipped_width
      state.gradient_end_ptr[base_idx + 1] = end_y / clipped_height

      # Calculate visible gradient range in the normalized coordinate space
      is_vertical = abs(end[1] - start[1]) > abs(end[0] - start[0])

      visible_start = 0.0
      visible_end = 1.0

      # No need to modify the visible gradient range calculation
      # since we're now properly transforming the gradient coordinates
      base_idx = i * 2
      state.visible_gradient_range_ptr[base_idx : base_idx + 2] = [visible_start, visible_end]


      # Set gradient colors
      colors = gradient.get('colors', [rl.WHITE])
      color_count = min(len(colors), MAX_GRADIENT_COLORS)

      state.color_counts_ptr[i] = color_count
      state.color_starts_ptr[i] = color_start

      for j, c in enumerate(colors[:color_count]):
        base_idx = (color_start + j) * 4

        # Handle both rl.Color objects and tuples/lists
        if hasattr(c, 'r'):
          # It's an rl.Color object
          state.gradient_colors_ptr[base_idx : base_idx + 4] = [
            c.r / 255.0, c.g / 255.0, c.b / 255.0, c.a / 255.0
          ]
        else:
          # Assume it's a tuple or list
          r, g, b, *a = c + (255,) if len(c) == 3 else c
          state.gradient_colors_ptr[base_idx : base_idx + 4] = [
            r / 255.0, g / 255.0, b / 255.0, a[0] / 255.0 if a else 1.0
          ]

      # Set gradient stops
      stops = gradient.get('stops', [j / (color_count - 1) for j in range(color_count)])
      for j, stop in enumerate(stops[:color_count]):
        state.gradient_stops_ptr[color_start + j] = stop

      color_start += color_count

  # Concatenate all points
  all_points = np.concatenate(all_transformed_points)

  # Set polygon count
  state.polygon_count_ptr[0] = polygon_count
  rl.set_shader_value(state.shader, state.locations['polygonCount'], state.polygon_count_ptr, UNIFORM_INT)

  # Set resolution
  state.resolution_ptr[0:2] = [clipped_width, clipped_height]
  rl.set_shader_value(state.shader, state.locations['resolution'], state.resolution_ptr, UNIFORM_VEC2)

  # Set points
  flat_points = np.ascontiguousarray(all_points.flatten().astype(np.float32))
  points_ptr = rl.ffi.cast("float *", flat_points.ctypes.data)
  rl.set_shader_value_v(state.shader, state.locations['points'], points_ptr, UNIFORM_VEC2, len(all_points))

  # Set point counts and starts
  rl.set_shader_value_v(
    state.shader, state.locations['pointCounts'], state.point_counts_ptr, UNIFORM_INT, polygon_count
  )
  rl.set_shader_value_v(
    state.shader, state.locations['pointStarts'], state.point_starts_ptr, UNIFORM_INT, polygon_count
  )

  # Set fill colors
  rl.set_shader_value_v(state.shader, state.locations['fillColors'], state.fill_colors_ptr, UNIFORM_VEC4, polygon_count)

  # Always set useGradient flags
  rl.set_shader_value_v(
    state.shader, state.locations['useGradient'], state.use_gradient_ptr, UNIFORM_INT, polygon_count
  )

  # Only set gradient data if we have at least one gradient
  if has_gradients:
    rl.set_shader_value_v(
      state.shader, state.locations['gradientStart'], state.gradient_start_ptr, UNIFORM_VEC2, polygon_count
    )
    rl.set_shader_value_v(
      state.shader, state.locations['gradientEnd'], state.gradient_end_ptr, UNIFORM_VEC2, polygon_count
    )
    rl.set_shader_value_v(
      state.shader,
      state.locations['visibleGradientRange'],
      state.visible_gradient_range_ptr,
      UNIFORM_VEC2,
      polygon_count,
    )
    rl.set_shader_value_v(
      state.shader, state.locations['gradientColorCounts'], state.color_counts_ptr, UNIFORM_INT, polygon_count
    )
    rl.set_shader_value_v(
      state.shader, state.locations['gradientColorStarts'], state.color_starts_ptr, UNIFORM_INT, polygon_count
    )

    # Set gradient colors and stops
    if color_start > 0:
      rl.set_shader_value_v(
        state.shader, state.locations['gradientColors'], state.gradient_colors_ptr, UNIFORM_VEC4, color_start
      )
      rl.set_shader_value_v(
        state.shader, state.locations['gradientStops'], state.gradient_stops_ptr, UNIFORM_FLOAT, color_start
      )

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


def draw_polygon(rect: rl.Rectangle, points: np.ndarray, color=None, gradient=None):
  """
  Draw a single polygon using the multi-polygon shader

  Args:
      rect: Rectangle defining the drawing area
      points: Array of (x,y) points defining the polygon
      color: Solid fill color (optional, defaults to WHITE)
      gradient: Gradient config (optional, overrides color if provided):
          {
              'start': (x1, y1),  # Start point (normalized 0-1)
              'end': (x2, y2),    # End point (normalized 0-1)
              'colors': [rl.Color], # List of colors at stops
              'stops': [float]    # List of positions (0-1)
          }
  """
  draw_polygons(rect, [{'points': points, 'color': color, 'gradient': gradient}])


def draw_lane_paths(rect, lane_polygons, confidence_values=None):
  """
  Draw multiple lane paths with different colors/gradients

  Args:
      rect: Rectangle defining the drawing area
      lane_polygons: List of np.ndarray points for each lane
      confidence_values: Optional list of confidence values (0-1) for each lane
  """
  if confidence_values is None:
    confidence_values = [0.8] * len(lane_polygons)

  polygon_configs = []

  for i, (points, confidence) in enumerate(zip(lane_polygons, confidence_values, strict=True)):
    # Skip invalid polygons
    if len(points) < 3:
      continue

    # Different colors for different lane types
    if i == 0:  # Left edge line
      base_color = rl.Color(255, 255, 0, int(255 * confidence * 0.7))
    elif i == 1:  # Right edge line
      base_color = rl.Color(255, 0, 0, int(255 * confidence * 0.7))
    else:  # Center lines
      base_color = rl.Color(255, 255, 255, int(255 * confidence * 0.7))

    # For higher confidence lanes, use gradient for better visual quality
    if confidence > 0.5:
      gradient = {
        'start': (0, 0),
        'end': (0, 1),  # Vertical gradient
        'colors': [base_color, rl.Color(base_color.r, base_color.g, base_color.b, int(base_color.a * 0.3))],
        'stops': [0.0, 1.0],
      }

      polygon_configs.append(
        {
          'points': points,
          'color': base_color,  # Fallback color
          'gradient': gradient,
        }
      )
    else:
      # For lower confidence, just use solid color (more efficient)
      polygon_configs.append({'points': points, 'color': base_color})

  # Draw all lanes in one call
  draw_polygons(rect, polygon_configs)
