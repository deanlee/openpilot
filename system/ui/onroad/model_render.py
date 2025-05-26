import numpy as np
import pyray as rl
from cereal import messaging, log
from typing import List, Tuple, Optional
from dataclasses import dataclass

# Constants matching the C++ implementation
CLIP_MARGIN = 500
MIN_DRAW_DISTANCE = 10.0
MAX_DRAW_DISTANCE = 100.0

# void DrawTexturePoly(Texture2D texture, Vector2 center, Vector2 *points, Vector2 *texcoords, int pointsCount, Color tint); // Draw a textured polygon

# the down side of this is that it is only suitable for shapes where there is a unobstructed line between the centre points and the points defining the polygon, theres a name for that sort of poly but I can't bring it to mind!


class ModelRenderer:
  """
  Renders road model visualization elements like path, lane lines,
  road edges, and lead vehicles. This is a Python implementation of
  the C++ ModelRenderer using PyRay instead of QPainter.
  """

  def __init__(self):
    self.longitudinal_control = False
    self.experimental_mode = False
    self.blend_factor = 1.0
    self.prev_allow_throttle = True
    self.lane_line_probs = [0.0] * 4
    self.road_edge_stds = [0.0] * 2
    self.path_offset_z = 1.22

    # Initialize empty polygon vertices
    self.track_vertices = []
    self.lane_line_vertices = [[] for _ in range(4)]
    self.road_edge_vertices = [[] for _ in range(2)]
    self.lead_vertices = [None, None]

    # Transform matrix (3x3 for car space to screen space)
    self.car_space_transform = np.zeros((3, 3))
    self.clip_region = None
    self.sm = None

    # Initialize shader for polygon rendering
    self.polygon_shader = self._load_polygon_shader()

  # def draw_polygon(self, points, color):
  #   # print('hhh')
  #   """Draw a filled polygon with even-odd fill rule using shader"""
  #   if len(points) < 3:
  #     return

  #   # For very simple polygons (3-4 vertices), use triangle fan directly
  #   if len(points) <= 4:
  #     vertices = []
  #     for p in points:
  #       vertices.append(rl.Vector2(p[0], p[1]))

  #     rl.draw_triangle_fan(vertices, len(vertices), color)

  #     # Draw outline
  #     outline_color = rl.Color(max(0, color.r - 15), max(0, color.g - 15), max(0, color.b - 15), color.a)
  #     for i in range(len(vertices)):
  #       start = vertices[i]
  #       end = vertices[(i + 1) % len(vertices)]
  #       rl.draw_line_ex(start, end, 1.0, outline_color)

  #     return

  #   # Find bounding box of polygon
  #   min_x = min(p[0] for p in points)
  #   min_y = min(p[1] for p in points)
  #   max_x = max(p[0] for p in points)
  #   max_y = max(p[1] for p in points)

  #   # Create a rectangle covering the polygon bounds
  #   rect_vertices = [
  #     rl.Vector2(min_x, min_y),
  #     rl.Vector2(max_x, min_y),
  #     rl.Vector2(max_x, max_y),
  #     rl.Vector2(min_x, max_y),
  #   ]

  #   # Create array of polygon vertices for shader
  #   point_count = min(len(points), 64)  # Limit to max vertices shader can handle
  #   poly_points = rl.ffi.new("float[]", point_count * 2)

  #   for i in range(point_count):
  #     poly_points[i * 2] = float(points[i][0])
  #     poly_points[i * 2 + 1] = float(points[i][1])

  #   # Begin shader mode
  #   rl.begin_shader_mode(self.polygon_shader)

  #   # Set shader uniforms
  #   rl.set_shader_value(self.polygon_shader, self.poly_points_loc, poly_points, rl.SHADER_UNIFORM_VEC2)
  #   rl.set_shader_value(
  #     self.polygon_shader, self.point_count_loc, rl.ffi.new("int[]", [point_count]), rl.SHADER_UNIFORM_INT
  #   )

  #   # Draw a rectangle that covers the polygon bounds
  #   # The shader will determine which pixels to fill based on even-odd rule
  #   rl.draw_triangle_fan(rect_vertices, 4, color)

  #   # End shader mode
  #   rl.end_shader_mode()

  # Function to calculate intersection x-coordinate for a given y
  def get_intersection_x(self, p1, p2, y):
      if p1[1] == p2[1]:  # Skip horizontal edges
          return None
      # Check if y is within the edge's y-range
      if (p1[1] <= y < p2[1]) or (p2[1] <= y < p1[1]):
          t = (y - p1[1]) / (p2[1] - p1[1])
          return p1[0] + t * (p2[0] - p1[0])
      return None

  # Function to fill the polygon using the scanline algorithm
  def fill_polygon(self, poly, color):

      n = len(poly)
      # Find the y-range of the polygon
      min_y = min(p[1] for p in poly)
      max_y = max(p[1] for p in poly)

      # Process each scanline
      for y in range(int(min_y), int(max_y) + 1):
          intersections = []
          # Calculate intersections with all edges
          for i in range(n):
              p1 = poly[i]
              p2 = poly[(i + 1) % n]  # Wrap around to the first point
              x = self.get_intersection_x(p1, p2, y)
              if x is not None:
                  intersections.append(x)

          # Sort intersections by x-coordinate
          intersections.sort()
          # Fill spans between pairs of intersections (odd-even rule)
          for i in range(0, len(intersections), 2):
              if i + 1 < len(intersections):  # Ensure we have a pair
                  x1 = int(intersections[i])
                  x2 = int(intersections[i + 1])
                  rl.draw_line(x1, y, x2, y, color)

  def draw_polygon(self, points, color):
    """Draw a filled polygon with even-odd fill rule using shader"""
    if len(points) < 3:
        return

    # For very simple polygons (3-4 vertices), use triangle fan directly
    if len(points) <= 4:
        vertices = []
        for p in points:
            vertices.append(rl.Vector2(p[0], p[1]))

        rl.draw_triangle_fan(vertices, len(vertices), color)

        # Draw outline
        outline_color = rl.Color(max(0, color.r - 15), max(0, color.g - 15), max(0, color.b - 15), color.a)
        for i in range(len(vertices)):
            start = vertices[i]
            end = vertices[(i + 1) % len(vertices)]
            rl.draw_line_ex(start, end, 1.0, outline_color)

        return

    # Check for self-intersection and nearly duplicate points
    # This is likely the path polygon with a reflection point
    if len(points) > 30:  # Typical path has many vertices
        # Find potential reflection point where x or y values start increasing again
        reflection_index = -1

        for i in range(1, len(points) // 2):
            # If points are very close together (potential reflection point)
            p1 = points[i]
            p2 = points[i+1]

            # Check for near duplication (points very close together)
            if abs(p1[0] - p2[0]) < 1.0 and abs(p1[1] - p2[1]) < 1.0:
                reflection_index = i
                break

            # Or if points start going back in direction
            elif i > 1:
                p0 = points[i-1]
                # Direction change detection
                if (p1[0] - p0[0]) * (p2[0] - p1[0]) < 0 or (p1[1] - p0[1]) * (p2[1] - p1[1]) < 0:
                    reflection_index = i
                    break

        # If we found a reflection point, draw as separate segments
        if reflection_index > 0:
            # Draw in segments to avoid self-intersection
            # Split into two halves at the midpoint
            mid_point = len(points) // 2

            # Create left and right side arrays
            left_side = points[:mid_point]
            right_side = points[mid_point:]

            # Draw segments from the outside in
            for i in range(min(len(left_side), len(right_side)) - 1):
                # Skip segments with potential issues
                if i == reflection_index or i + 1 == reflection_index:
                    continue

                # Create a quad segment
                segment = [
                    left_side[i],
                    left_side[i+1],
                    right_side[-(i+2)],
                    right_side[-(i+1)]
                ]

                # Draw quad
                quad_vertices = []
                for p in segment:
                    quad_vertices.append(rl.Vector2(p[0], p[1]))

                rl.draw_triangle_fan(quad_vertices, len(quad_vertices), color)

            # Draw outline for the entire polygon
            vertices = []
            for p in points:
                vertices.append(rl.Vector2(p[0], p[1]))

            outline_color = rl.Color(max(0, color.r - 15), max(0, color.g - 15), max(0, color.b - 15), color.a)
            for i in range(len(vertices)):
                start = vertices[i]
                end = vertices[(i + 1) % len(vertices)]
                rl.draw_line_ex(start, end, 1.0, outline_color)

            return

    # For complex lane line or path polygons, use a segmented approach
    if len(points) > 15:  # Likely a lane line or complex path
        # Find if the points form left/right sides (lane or path structure)
        mid_point = len(points) // 2

        # Try to detect if this is a properly formed lane-like polygon
        # (first half ascending, second half descending or vice versa)
        is_lane_like = False
        if len(points) >= 6:
            left_side = points[:mid_point]
            right_side = points[mid_point:]

            # Check if the sides form a proper structure
            if (len(left_side) > 1 and len(right_side) > 1):
                # Check if sides have opposite y-direction
                left_direction = left_side[-1][1] - left_side[0][1]
                right_direction = right_side[-1][1] - right_side[0][1]

                if left_direction * right_direction < 0:  # Opposite directions
                    is_lane_like = True

        if is_lane_like:
            # Draw lane with segments for better control
            segments = []
            max_segments = min(mid_point - 1, len(points) - mid_point - 1)

            for i in range(max_segments):
                segment = [
                    points[i],
                    points[i+1],
                    points[len(points)-i-2],
                    points[len(points)-i-1]
                ]
                segments.append(segment)

            # Draw segments
            for segment in segments:
                # Convert to vector2
                vertices = []
                for p in segment:
                    vertices.append(rl.Vector2(p[0], p[1]))

                rl.draw_triangle_fan(vertices, len(vertices), color)

            # Draw outline for the entire polygon
            vertices = []
            for p in points:
                vertices.append(rl.Vector2(p[0], p[1]))

            outline_color = rl.Color(max(0, color.r - 15), max(0, color.g - 15), max(0, color.b - 15), color.a)
            for i in range(len(vertices)):
                start = vertices[i]
                end = vertices[(i + 1) % len(vertices)]
                rl.draw_line_ex(start, end, 1.0, outline_color)

            return

    # For regular polygons, use the shader-based approach
    # Find bounding box of polygon
    min_x = min(p[0] for p in points)
    min_y = min(p[1] for p in points)
    max_x = max(p[0] for p in points)
    max_y = max(p[1] for p in points)

    # Expand bounds slightly to ensure coverage
    padding = 1.0
    min_x -= padding
    min_y -= padding
    max_x += padding
    max_y += padding

    # Create a rectangle covering the polygon bounds
    rect_vertices = [
        rl.Vector2(min_x, min_y),
        rl.Vector2(max_x, min_y),
        rl.Vector2(max_x, max_y),
        rl.Vector2(min_x, max_y),
    ]

    # Create array of polygon vertices for shader
    point_count = min(len(points), 64)  # Limit to max vertices shader can handle
    poly_points = rl.ffi.new("float[]", point_count * 2)

    for i in range(point_count):
        poly_points[i * 2] = float(points[i][0])
        poly_points[i * 2 + 1] = float(points[i][1])

    # Begin shader mode
    rl.begin_shader_mode(self.polygon_shader)

    # Set shader uniforms
    rl.set_shader_value(self.polygon_shader, self.poly_points_loc, poly_points, rl.SHADER_UNIFORM_VEC2)
    rl.set_shader_value(
        self.polygon_shader, self.point_count_loc, rl.ffi.new("int[]", [point_count]), rl.SHADER_UNIFORM_INT
    )

    # Draw a rectangle that covers the polygon bounds
    # The shader will determine which pixels to fill based on even-odd rule
    rl.draw_triangle_fan(rect_vertices, 4, color)

    # End shader mode
    rl.end_shader_mode()

    # Draw outline for better definition
    vertices = []
    for p in points[:point_count]:
        vertices.append(rl.Vector2(p[0], p[1]))

    outline_color = rl.Color(max(0, color.r - 15), max(0, color.g - 15), max(0, color.b - 15), color.a)
    for i in range(len(vertices)):
        start = vertices[i]
        end = vertices[(i + 1) % len(vertices)]
        rl.draw_line_ex(start, end, 1.0, outline_color)

  def _load_polygon_shader(self):
    """Load shader for polygon rendering with even-odd fill rule"""
    # Vertex shader passes position for point-in-polygon test
    vs_code = """
      #version 330
      in vec3 vertexPosition;
      in vec4 vertexColor;
      in vec2 vertexTexCoord;

      uniform mat4 mvp;

      out vec4 fragColor;
      out vec2 fragPos;

      void main() {
          gl_Position = mvp * vec4(vertexPosition, 1.0);
          fragColor = vertexColor;
          fragPos = vertexPosition.xy;  // Pass position to fragment shader
      }
      """

    # Fragment shader implements point-in-polygon test
    fs_code = """
      #version 330
      in vec4 fragColor;
      in vec2 fragPos;

      uniform vec2 polyPoints[64];  // Max vertices in polygon
      uniform int pointCount;

      out vec4 finalColor;

      void main() {
          // Implement even-odd fill rule using ray casting algorithm
          bool inside = false;

          // Cast ray from current fragment to the right
          for (int i = 0, j = pointCount - 1; i < pointCount; j = i++) {
              // Check if edge crosses horizontal ray from point
              if (((polyPoints[i].y > fragPos.y) != (polyPoints[j].y > fragPos.y)) &&
                  (fragPos.x < polyPoints[i].x + (polyPoints[j].x - polyPoints[i].x) *
                  (fragPos.y - polyPoints[i].y) / (polyPoints[j].y - polyPoints[i].y))) {
                  inside = !inside;
              }
          }

          // Set color based on whether point is inside polygon
          if (inside) {
              finalColor = fragColor;
          } else {
              discard;  // Don't draw outside polygon
          }
      }
      """

    # Create shader
    shader = rl.load_shader_from_memory(vs_code, fs_code)

    # Get shader uniform locations for later use
    self.poly_points_loc = rl.get_shader_location(shader, "polyPoints")
    self.point_count_loc = rl.get_shader_location(shader, "pointCount")

    return shader

  def set_transform(self, transform: np.ndarray):
    """Set the transformation matrix from car space to screen space"""
    self.car_space_transform = transform
    # print(transform)

  def draw(self, rect: rl.Rectangle, sm: messaging.SubMaster):
    """
    Draw the model visualization (lane lines, path, lead vehicles)

    Args:
        rect: The rectangle to draw within
        sm: SubMaster instance with required messages
    """
    self.sm = sm
    # Check if data is up-to-date
    if not sm.valid['modelV2'] or not sm.valid['liveCalibration']:
      return

    # Set up clipping region
    self.clip_region = rl.Rectangle(
      rect.x - CLIP_MARGIN, rect.y - CLIP_MARGIN, rect.width + 2 * CLIP_MARGIN, rect.height + 2 * CLIP_MARGIN
    )

    # Update flags based on car state
    self.experimental_mode = sm['selfdriveState'].experimentalMode
    self.longitudinal_control = sm['carParams'].openpilotLongitudinalControl
    self.path_offset_z = sm['liveCalibration'].height[0]

    # Get model and radar data
    model = sm['modelV2']
    radar_state = sm['radarState'] if sm.valid['radarState'] else None
    lead_one = radar_state.leadOne if radar_state else None

    # Update model data
    self.update_model(model, lead_one)

    # Draw elements
    self.draw_lane_lines()
    self.draw_path(model, rect.height)

    # Draw lead vehicles if available
    if self.longitudinal_control and radar_state:
      self.update_leads(radar_state, model.position)
      lead_two = radar_state.leadTwo

      if lead_one and lead_one.status:
        self.draw_lead(lead_one, self.lead_vertices[0], rect)

      if lead_two and lead_two.status and lead_one and (abs(lead_one.dRel - lead_two.dRel) > 3.0):
        self.draw_lead(lead_two, self.lead_vertices[1], rect)

  def update_leads(self, radar_state, line):
    """Update positions of lead vehicles"""
    for i in range(2):
      lead_data = radar_state.leadOne if i == 0 else radar_state.leadTwo
      if lead_data and lead_data.status:
        idx = self.get_path_length_idx(line, lead_data.dRel)
        z = line.z[idx]
        self.lead_vertices[i] = self.map_to_screen(lead_data.dRel, -lead_data.yRel, z + self.path_offset_z)

  def update_model(self, model, lead):
    """Update model visualization data based on model message"""
    model_position = model.position

    # Determine max distance to render
    max_distance = np.clip(model_position.x[-1], MIN_DRAW_DISTANCE, MAX_DRAW_DISTANCE)

    # Update lane lines
    lane_lines = model.laneLines
    line_probs = model.laneLineProbs
    max_idx = self.get_path_length_idx(lane_lines[0], max_distance)

    for i in range(4):
      self.lane_line_probs[i] = line_probs[i]
      self.lane_line_vertices[i] = self.map_line_to_polygon(lane_lines[i], 0.025 * self.lane_line_probs[i], 0, max_idx)

    # Update road edges
    road_edges = model.roadEdges
    edge_stds = model.roadEdgeStds

    for i in range(2):
      self.road_edge_stds[i] = edge_stds[i]
      self.road_edge_vertices[i] = self.map_line_to_polygon(road_edges[i], 0.025, 0, max_idx)

    # Update path
    if lead and lead.status:
      lead_d = lead.dRel * 2.0
      max_distance = np.clip(lead_d - min(lead_d * 0.35, 10.0), 0.0, max_distance)

    max_idx = self.get_path_length_idx(model_position, max_distance)
    self.track_vertices = self.map_line_to_polygon(model_position, 0.9, self.path_offset_z, max_idx, False)

  def draw_lane_lines(self):
    """Draw lane lines and road edges"""
    for i in range(4):
      # Skip if no vertices
      if not self.lane_line_vertices[i]:
        continue

      # Draw lane line
      alpha = np.clip(self.lane_line_probs[i], 0.0, 0.7)
      color = rl.Color(255, 255, 255, int(alpha * 255))
      self.draw_polygon(self.lane_line_vertices[i], color)

    for i in range(2):
      # Skip if no vertices
      if not self.road_edge_vertices[i]:
        continue

      # Draw road edge
      alpha = np.clip(1.0 - self.road_edge_stds[i], 0.0, 1.0)
      color = rl.Color(255, 0, 0, int(alpha * 255))
      # color = rl.ColorAlpha(rl.RED, alpha)
      self.draw_polygon(self.road_edge_vertices[i], color)

  def draw_path(self, model, height):
    """Draw the path polygon with gradient based on acceleration"""
    if not self.track_vertices:
      return

    if self.experimental_mode:
      # Draw with acceleration coloring
      acceleration = model.acceleration.x
      max_len = min(len(self.track_vertices) // 2, len(acceleration))

      # Find midpoint index for polygon
      mid_point = len(self.track_vertices) // 2

      # For acceleration-based coloring, process segments separately
      left_side = self.track_vertices[:mid_point]
      right_side = self.track_vertices[mid_point:][::-1]  # Reverse for proper winding

      # Create segments for gradient coloring
      segments = []
      segment_colors = []

      for i in range(max_len - 1):
        if i >= len(left_side) - 1 or i >= len(right_side) - 1:
          break

        track_idx = max_len - i - 1  # flip idx to start from bottom right

        # Skip points out of frame
        if left_side[track_idx][1] < 0 or left_side[track_idx][1] > height:
          continue

        # Calculate color based on acceleration
        lin_grad_point = (height - left_side[track_idx][1]) / height

        # speed up: 120, slow down: 0
        path_hue = max(min(60 + acceleration[i] * 35, 120), 0)
        path_hue = int(path_hue * 100 + 0.5) / 100

        saturation = min(abs(acceleration[i] * 1.5), 1)
        lightness = self.map_val(saturation, 0.0, 1.0, 0.95, 0.62)
        alpha = self.map_val(lin_grad_point, 0.75 / 2.0, 0.75, 0.4, 0.0)

        # Use HSL to RGB conversion
        color = self.hsla_to_color(path_hue / 360.0, saturation, lightness, alpha)

        # Create quad segment
        segment = [left_side[track_idx], left_side[track_idx - 1], right_side[track_idx - 1], right_side[track_idx]]

        segments.append(segment)
        segment_colors.append(color)

      # Draw each segment with its color
      for i, (segment, color) in enumerate(zip(segments, segment_colors)):
        self.draw_polygon(segment, color)
    else:
      # Draw with throttle/no throttle gradient
      # Get throttle state
      allow_throttle = self.sm['longitudinalPlan'].allowThrottle or not self.longitudinal_control

      # Start transition if throttle state changes
      if allow_throttle != self.prev_allow_throttle:
        self.prev_allow_throttle = allow_throttle
        self.blend_factor = max(1.0 - self.blend_factor, 0.0)

      # Update blend factor
      if self.blend_factor < 1.0:
        self.blend_factor = min(self.blend_factor + 0.1, 1.0)

      # Define gradient colors
      throttle_colors = [
        self.hsla_to_color(148 / 360, 0.94, 0.51, 0.4),
        self.hsla_to_color(112 / 360, 1.0, 0.68, 0.35),
        self.hsla_to_color(112 / 360, 1.0, 0.68, 0.0),
      ]

      no_throttle_colors = [
        self.hsla_to_color(148 / 360, 0.0, 0.95, 0.4),
        self.hsla_to_color(112 / 360, 0.0, 0.95, 0.35),
        self.hsla_to_color(112 / 360, 0.0, 0.95, 0.0),
      ]

      begin_colors = no_throttle_colors if allow_throttle else throttle_colors
      end_colors = throttle_colors if allow_throttle else no_throttle_colors

      # Blend colors based on transition
      colors = [
        self.blend_colors(begin_colors[0], end_colors[0], self.blend_factor),
        self.blend_colors(begin_colors[1], end_colors[1], self.blend_factor),
        self.blend_colors(begin_colors[2], end_colors[2], self.blend_factor),
      ]

      # Draw path with gradient
      self.draw_polygon_gradient(self.track_vertices, colors)

  def draw_lead(self, lead_data, vd, rect):
    """Draw lead vehicle indicator"""
    if not vd:
      return

    speed_buff = 10.0
    lead_buff = 40.0
    d_rel = lead_data.dRel
    v_rel = lead_data.vRel

    # Calculate fill alpha
    fill_alpha = 0
    if d_rel < lead_buff:
      fill_alpha = 255 * (1.0 - (d_rel / lead_buff))
      if v_rel < 0:
        fill_alpha += 255 * (-1 * (v_rel / speed_buff))
      fill_alpha = min(fill_alpha, 255)

    # Calculate size and position
    sz = np.clip((25 * 30) / (d_rel / 3 + 30), 15.0, 30.0) * 2.35
    x = np.clip(vd[0], 0.0, rect.width - sz / 2)
    y = min(vd[1], rect.height - sz * 0.6)

    g_xo = sz / 5
    g_yo = sz / 10

    # Draw glow
    glow = [(x + (sz * 1.35) + g_xo, y + sz + g_yo), (x, y - g_yo), (x - (sz * 1.35) - g_xo, y + sz + g_yo)]
    self.draw_polygon(glow, rl.Color(218, 202, 37, 255))

    # Draw chevron
    chevron = [(x + (sz * 1.25), y + sz), (x, y), (x - (sz * 1.25), y + sz)]
    self.draw_polygon(chevron, rl.Color(201, 34, 49, int(fill_alpha)))

  def get_path_length_idx(self, line, path_height):
    """Get the index corresponding to the given path height"""
    line_x = line.x
    max_idx = 0
    for i in range(1, len(line_x)):
      if line_x[i] <= path_height:
        max_idx = i
      else:
        break
    return max_idx

  def map_to_screen(self, in_x, in_y, in_z):
    """Project a point in car space to screen space"""
    input_pt = np.array([in_x, in_y, in_z])
    pt = self.car_space_transform @ input_pt

    # Return 2D point with perspective divide
    screen_pt = (pt[0] / pt[2], pt[1] / pt[2])

    # return screen_pt
    # Check if point is in clip region
    if (
      self.clip_region.x <= screen_pt[0] <= self.clip_region.x + self.clip_region.width
      and self.clip_region.y <= screen_pt[1] <= self.clip_region.y + self.clip_region.height
    ):
      return screen_pt
    return None

  # def map_line_to_polygon(self, line, y_off, z_off, max_idx, allow_invert=True):
  #   """Convert a 3D line to a 2D polygon for drawing"""
  #   line_x = line.x
  #   line_y = line.y
  #   line_z = line.z

  #   points = []
  #   for i in range(max_idx + 1):
  #     # Skip points with negative x (behind camera)
  #     if line_x[i] < 0:
  #       continue

  #     left = self.map_to_screen(line_x[i], line_y[i] - y_off, line_z[i] + z_off)
  #     right = self.map_to_screen(line_x[i], line_y[i] + y_off, line_z[i] + z_off)

  #     if left and right:
  #       # Check for inversion when going over hills
  #       if not allow_invert and points and left[1] > points[-1][1]:
  #         continue

  #       # Add points to create a polygon
  #       points.append(left)
  #       points.insert(0, right)

  #   return points

  def map_line_to_polygon(self, line, y_off, z_off, max_idx, allow_invert=True):
    """Convert a 3D line to a 2D polygon for drawing"""
    line_x = line.x
    line_y = line.y
    line_z = line.z

    # Store left and right sides separately
    left_points = []
    right_points = []

    for i in range(max_idx + 1):
        # Skip points with negative x (behind camera)
        if line_x[i] < 0:
            continue

        left = self.map_to_screen(line_x[i], line_y[i] - y_off, line_z[i] + z_off)
        right = self.map_to_screen(line_x[i], line_y[i] + y_off, line_z[i] + z_off)

        if left and right:
            # Check for inversion when going over hills
            if not allow_invert and left_points and left[1] > left_points[-1][1]:
                continue

            # Add points to their respective sides
            left_points.append(left)
            right_points.append(right)

    # Create final polygon by correctly ordering the points
    points = []
    points.extend(left_points)          # Left side from front to back
    points.extend(right_points[::-1])   # Right side from back to front
    print(points)
    return points

  @staticmethod
  def map_val(x, x0, x1, y0, y1):
    """Map value x from range [x0, x1] to range [y0, y1]"""
    return y0 + (y1 - y0) * ((x - x0) / (x1 - x0)) if x1 != x0 else y0

  @staticmethod
  def hsla_to_color(h, s, l, a):
    """Convert HSLA color to Raylib Color"""
    # Convert HSL to RGB
    if s == 0:
      r = g = b = l
    else:

      def hue_to_rgb(p, q, t):
        if t < 0:
          t += 1
        if t > 1:
          t -= 1
        if t < 1 / 6:
          return p + (q - p) * 6 * t
        if t < 1 / 2:
          return q
        if t < 2 / 3:
          return p + (q - p) * (2 / 3 - t) * 6
        return p

      q = l * (1 + s) if l < 0.5 else l + s - l * s
      p = 2 * l - q
      r = hue_to_rgb(p, q, h + 1 / 3)
      g = hue_to_rgb(p, q, h)
      b = hue_to_rgb(p, q, h - 1 / 3)

    # Convert to 8-bit color values
    return rl.Color(int(r * 255), int(g * 255), int(b * 255), int(a * 255))

  @staticmethod
  def blend_colors(start, end, t):
    """Blend between two colors with factor t"""
    if t >= 1.0:
      return end

    return rl.Color(
      int((1 - t) * start.r + t * end.r),
      int((1 - t) * start.g + t * end.g),
      int((1 - t) * start.b + t * end.b),
      int((1 - t) * start.a + t * end.a),
    )

  def is_simple_polygon(self, points):
    """Check if this is likely a simple convex or nearly-convex polygon"""
    # Simple heuristic: check if polygon is roughly convex
    # by calculating cross products of consecutive edges
    if len(points) < 4:
      return True

    last_sign = None
    sign_changes = 0

    for i in range(len(points)):
      a = points[i]
      b = points[(i + 1) % len(points)]
      c = points[(i + 2) % len(points)]

      # Cross product to determine convexity
      cross = (b[0] - a[0]) * (c[1] - b[1]) - (b[1] - a[1]) * (c[0] - b[0])

      # Check sign change
      current_sign = cross > 0
      if last_sign is not None and current_sign != last_sign:
        sign_changes += 1
      last_sign = current_sign

    # If only a few sign changes, it's mostly convex
    return sign_changes <= 2

  def draw_polygon_gradient(self, points, colors):
    """Draw a filled polygon with gradient coloring using shader"""
    if len(points) < 3:
      return

    # For simple polygons, use the built-in gradient methods
    if len(points) <= 4:
      vertices = []
      for p in points:
        vertices.append(rl.Vector2(p[0], p[1]))

      # Find midpoint
      mid_x = sum(p[0] for p in points) / len(points)
      mid_y = sum(p[1] for p in points) / len(points)

      # Draw gradient rectangle using Raylib's built-in function
      rl.draw_rectangle_gradient_v(
        int(min(p[0] for p in points)),
        int(min(p[1] for p in points)),
        int(max(p[0] for p in points) - min(p[0] for p in points)),
        int(max(p[1] for p in points) - min(p[1] for p in points)),
        colors[0],
        colors[-1],
      )

      # Clip to polygon shape by using stencil buffer
      rl.rl_enable_stencil_test()
      rl.rl_clear_stencil_buffer(0)
      rl.rl_stencil_func(0x0207, 1, 0xFF)  # GL_EQUAL
      rl.rl_stencil_op(0x1E00, 0x1E00, 0x1E01)  # GL_KEEP, GL_KEEP, GL_REPLACE

      # Draw polygon to stencil buffer
      rl.draw_triangle_fan(vertices, len(vertices), rl.Color(255, 255, 255, 255))

      # Draw outline
      outline_color = rl.Color(
        max(0, colors[0].r - 15), max(0, colors[0].g - 15), max(0, colors[0].b - 15), colors[0].a
      )
      for i in range(len(vertices)):
        start = vertices[i]
        end = vertices[(i + 1) % len(vertices)]
        rl.draw_line_ex(start, end, 1.0, outline_color)

      rl.rl_disable_stencil_test()
      return

    # For path polygon with gradient, use a special approach:
    # Split the polygon into multiple segments and color each with gradient steps
    if len(points) >= 6:  # Path-like polygon with enough points for segments
      # Assume path polygon has left side and right side like your track_vertices
      mid_point = len(points) // 2
      left_side = points[:mid_point]
      right_side = points[mid_point:][::-1]  # Reverse right side

      # Create segments
      segments = []
      for i in range(len(left_side) - 1):
        if i < len(right_side) - 1:
          segment = [left_side[i], left_side[i + 1], right_side[i + 1], right_side[i]]
          segments.append(segment)

      # Calculate color for each segment
      segment_colors = []
      for i in range(len(segments)):
        t = i / max(1, len(segments) - 1)
        # Interpolate between gradient colors
        if len(colors) == 2:
          color = self.blend_colors(colors[0], colors[1], t)
        elif len(colors) == 3:
          if t < 0.5:
            color = self.blend_colors(colors[0], colors[1], t * 2)
          else:
            color = self.blend_colors(colors[1], colors[2], (t - 0.5) * 2)
        else:
          color = colors[0]  # Default to first color
        segment_colors.append(color)

      # Draw each segment with its color
      for i, segment in enumerate(segments):
        self.draw_polygon(segment, segment_colors[i])

      return

    # Fall back to non-gradient fill for complex polygons
    self.draw_polygon(points, colors[0])
