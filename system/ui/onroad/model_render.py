import numpy as np
import pyray as rl
from cereal import messaging, log
from typing import List, Tuple, Optional
from dataclasses import dataclass


# Constants matching the C++ implementation
CLIP_MARGIN = 500
MIN_DRAW_DISTANCE = 10.0
MAX_DRAW_DISTANCE = 100.0


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

  def set_transform(self, transform: np.ndarray):
    """Set the transformation matrix from car space to screen space"""
    self.car_space_transform = transform

  def draw(self, rect: rl.Rectangle, sm: messaging.SubMaster):
    """
    Draw the model visualization (lane lines, path, lead vehicles)

    Args:
        rect: The rectangle to draw within
        sm: SubMaster instance with required messages
    """
    # Check if data is up-to-date
    if not sm.valid.get('modelV2') or not sm.valid.get('liveCalibration'):
      return

    # Set up clipping region
    self.clip_region = rl.Rectangle(
      rect.x - CLIP_MARGIN, rect.y - CLIP_MARGIN, rect.width + 2 * CLIP_MARGIN, rect.height + 2 * CLIP_MARGIN
    )

    # Update flags based on car state
    self.experimental_mode = sm['selfdriveState'].getSelfdriveState().getExperimentalMode()
    self.longitudinal_control = sm['carParams'].getCarParams().getOpenpilotLongitudinalControl()
    self.path_offset_z = sm['liveCalibration'].getLiveCalibration().getHeight()[0]

    # Get model and radar data
    model = sm['modelV2'].getModelV2()
    radar_state = sm['radarState'].getRadarState() if sm.valid.get('radarState') else None
    lead_one = radar_state.getLeadOne() if radar_state else None

    # Update model data
    self.update_model(model, lead_one)

    # Draw elements
    self.draw_lane_lines()
    self.draw_path(model, rect.height)

    # Draw lead vehicles if available
    if self.longitudinal_control and radar_state:
      self.update_leads(radar_state, model.getPosition())
      lead_two = radar_state.getLeadTwo()

      if lead_one and lead_one.getStatus():
        self.draw_lead(lead_one, self.lead_vertices[0], rect)

      if lead_two and lead_two.getStatus() and lead_one and (abs(lead_one.getDRel() - lead_two.getDRel()) > 3.0):
        self.draw_lead(lead_two, self.lead_vertices[1], rect)

  def update_leads(self, radar_state, line):
    """Update positions of lead vehicles"""
    for i in range(2):
      lead_data = radar_state.getLeadOne() if i == 0 else radar_state.getLeadTwo()
      if lead_data and lead_data.getStatus():
        idx = self.get_path_length_idx(line, lead_data.getDRel())
        z = line.getZ()[idx]
        self.lead_vertices[i] = self.map_to_screen(lead_data.getDRel(), -lead_data.getYRel(), z + self.path_offset_z)

  def update_model(self, model, lead):
    """Update model visualization data based on model message"""
    model_position = model.getPosition()

    # Determine max distance to render
    max_distance = np.clip(model_position.getX()[-1], MIN_DRAW_DISTANCE, MAX_DRAW_DISTANCE)

    # Update lane lines
    lane_lines = model.getLaneLines()
    line_probs = model.getLaneLineProbs()
    max_idx = self.get_path_length_idx(lane_lines[0], max_distance)

    for i in range(4):
      self.lane_line_probs[i] = line_probs[i]
      self.lane_line_vertices[i] = self.map_line_to_polygon(lane_lines[i], 0.025 * self.lane_line_probs[i], 0, max_idx)

    # Update road edges
    road_edges = model.getRoadEdges()
    edge_stds = model.getRoadEdgeStds()

    for i in range(2):
      self.road_edge_stds[i] = edge_stds[i]
      self.road_edge_vertices[i] = self.map_line_to_polygon(road_edges[i], 0.025, 0, max_idx)

    # Update path
    if lead and lead.getStatus():
      lead_d = lead.getDRel() * 2.0
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
      color = rl.ColorAlpha(rl.WHITE, alpha)
      self.draw_polygon(self.lane_line_vertices[i], color)

    for i in range(2):
      # Skip if no vertices
      if not self.road_edge_vertices[i]:
        continue

      # Draw road edge
      alpha = np.clip(1.0 - self.road_edge_stds[i], 0.0, 1.0)
      color = rl.ColorAlpha(rl.RED, alpha)
      self.draw_polygon(self.road_edge_vertices[i], color)

  def draw_path(self, model, height):
    """Draw the path polygon with gradient based on acceleration"""
    if not self.track_vertices:
      return

    if self.experimental_mode:
      # Draw with acceleration coloring
      acceleration = model.getAcceleration().getX()
      max_len = min(len(self.track_vertices) // 2, len(acceleration))

      # Create gradient colors for path sections
      for i in range(max_len):
        track_idx = max_len - i - 1  # flip idx to start from bottom right

        # Skip points out of frame
        if self.track_vertices[track_idx][1] < 0 or self.track_vertices[track_idx][1] > height:
          continue

        # Calculate color based on acceleration
        lin_grad_point = (height - self.track_vertices[track_idx][1]) / height

        # speed up: 120, slow down: 0
        path_hue = max(min(60 + acceleration[i] * 35, 120), 0)
        path_hue = int(path_hue * 100 + 0.5) / 100

        saturation = min(abs(acceleration[i] * 1.5), 1)
        lightness = self.map_val(saturation, 0.0, 1.0, 0.95, 0.62)
        alpha = self.map_val(lin_grad_point, 0.75 / 2.0, 0.75, 0.4, 0.0)

        # Use HSL to RGB conversion
        color = self.hsla_to_color(path_hue / 360.0, saturation, lightness, alpha)

        # Draw segment
        # (This is simplified - a full implementation would create a gradient fill)
        segment = self.track_vertices[track_idx : track_idx + 2] + self.track_vertices[-track_idx - 2 : -track_idx]
        self.draw_polygon(segment, color)

        # Skip a point, unless next is last
        i += 1 if i + 2 < max_len else 0
    else:
      # Draw with throttle/no throttle gradient
      # Get throttle state
      allow_throttle = False
      if 'longitudinalPlan' in sm:
        allow_throttle = (
          sm['longitudinalPlan'].getLongitudinalPlan().getAllowThrottle() or not self.longitudinal_control
        )

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
      # (Simplified implementation - would need more complex gradient in real implementation)
      self.draw_polygon(self.track_vertices, colors[0])

  def draw_lead(self, lead_data, vd, rect):
    """Draw lead vehicle indicator"""
    if not vd:
      return

    speed_buff = 10.0
    lead_buff = 40.0
    d_rel = lead_data.getDRel()
    v_rel = lead_data.getVRel()

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
    line_x = line.getX()
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

    # Check if point is in clip region
    if (
      self.clip_region.x <= screen_pt[0] <= self.clip_region.x + self.clip_region.width
      and self.clip_region.y <= screen_pt[1] <= self.clip_region.y + self.clip_region.height
    ):
      return screen_pt
    return None

  def map_line_to_polygon(self, line, y_off, z_off, max_idx, allow_invert=True):
    """Convert a 3D line to a 2D polygon for drawing"""
    line_x = line.getX()
    line_y = line.getY()
    line_z = line.getZ()

    points = []
    for i in range(max_idx + 1):
      # Skip points with negative x (behind camera)
      if line_x[i] < 0:
        continue

      left = self.map_to_screen(line_x[i], line_y[i] - y_off, line_z[i] + z_off)
      right = self.map_to_screen(line_x[i], line_y[i] + y_off, line_z[i] + z_off)

      if left and right:
        # Check for inversion when going over hills
        if not allow_invert and points and left[1] > points[-1][1]:
          continue

        # Add points to create a polygon
        points.append(left)
        points.insert(0, right)

    return points

  def draw_polygon(self, points, color):
    """Draw a filled polygon with the given points and color"""
    if len(points) < 3:
      return

    # Convert to Raylib Vector2 array
    vertices = []
    for p in points:
      vertices.append(rl.Vector2(p[0], p[1]))

    # Draw filled polygon
    rl.draw_poly_ex(vertices, len(vertices), color)

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
