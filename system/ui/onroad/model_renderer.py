import colorsys
import bisect
import numpy as np
import pyray as rl
from cereal import messaging, car
from openpilot.common.params import Params
from openpilot.system.ui.lib.shader_polygon import draw_polygon
# from openpilot.system.ui.lib.application import gui_app
from dataclasses import dataclass

CLIP_MARGIN = 500
MIN_DRAW_DISTANCE = 10.0
MAX_DRAW_DISTANCE = 100.0


THROTTLE_COLORS = [
  rl.Color(0, 231, 130, 102),  # Green with alpha 0.4
  rl.Color(112, 247, 35, 89),  # Lime with alpha 0.35
  rl.Color(112, 247, 35, 0),   # Transparent lime
]

NO_THROTTLE_COLORS = [
  rl.Color(242, 242, 242, 102),  # Light gray with alpha 0.4
  rl.Color(242, 242, 242, 89),   # Light gray with alpha 0.35
  rl.Color(242, 242, 242, 0),    # Transparent light gray
]
import time

@dataclass
class ModelPoints:
  raw_points: np.ndarray
  transformed_points: np.ndarray


class ModelRenderer:
  def __init__(self):
    self._longitudinal_control = False
    self._experimental_mode = False
    self._blend_factor = 1.0
    self._prev_allow_throttle = True
    self._lane_line_probs = np.zeros(4, dtype=np.float32)
    self._road_edge_stds = np.zeros(2, dtype=np.float32)
    self._path_offset_z = 1.22

    # Initialize ModelPoints objects
    self._track = ModelPoints(
      raw_points=np.empty((0, 3), dtype=np.float32),
      transformed_points=np.empty((0, 2), dtype=np.float32)
    )
    self._lane_lines = [
      ModelPoints(
        raw_points=np.empty((0, 3), dtype=np.float32),
        transformed_points=np.empty((0, 2), dtype=np.float32)
      ) for _ in range(4)
    ]
    self._road_edges = [
      ModelPoints(
        raw_points=np.empty((0, 3), dtype=np.float32),
        transformed_points=np.empty((0, 2), dtype=np.float32)
      ) for _ in range(2)
    ]
    self._lead_vertices = [None, None]

    # Transform matrix (3x3 for car space to screen space)
    self._car_space_transform = np.zeros((3, 3))
    self._transform_dirty = True
    self._clip_region = None

    # Get longitudinal control setting from car parameters
    car_params = Params().get("CarParams")
    if car_params:
      cp = messaging.log_from_bytes(car_params, car.CarParams)
      self._longitudinal_control = cp.openpilotLongitudinalControl

  def set_transform(self, transform: np.ndarray):
    self._car_space_transform = transform
    self._transform_dirty = True

  def draw(self, rect: rl.Rectangle, sm: messaging.SubMaster):
    # Check if data is up-to-date
    if not sm.valid['modelV2'] or not sm.valid['liveCalibration']:
      return

    # Set up clipping region
    self._clip_region = rl.Rectangle(
      rect.x - CLIP_MARGIN, rect.y - CLIP_MARGIN, rect.width + 2 * CLIP_MARGIN, rect.height + 2 * CLIP_MARGIN
    )

    # Update flags based on car state
    self._experimental_mode = sm['selfdriveState'].experimentalMode
    self._path_offset_z = sm['liveCalibration'].height[0]
    if sm.updated['carParams']:
      self._longitudinal_control = sm['carParams'].openpilotLongitudinalControl

    # Get model and radar data
    model = sm['modelV2']
    radar_state = sm['radarState'] if sm.valid['radarState'] else None
    lead_one = radar_state.leadOne if radar_state else None
    render_lead_indicator = self._longitudinal_control and radar_state is not None

    # Update model data when needed
    model_updated = sm.updated['modelV2']
    if model_updated or sm.updated['radarState'] or self._transform_dirty:
      if model_updated:
        self._update_raw_points(model)

      pos_x_array = self._track.raw_points[:, 0]
      if pos_x_array.size == 0:
        return

      self._update_model(lead_one, pos_x_array)
      if render_lead_indicator:
        self._update_leads(radar_state, pos_x_array)
      self._transform_dirty = False


    # Draw elements
    self._draw_lane_lines()
    self._draw_path(sm, model, rect.height)

    # Draw lead vehicles if available
    if render_lead_indicator and radar_state:
      lead_two = radar_state.leadTwo

      if lead_one and lead_one.status:
        self._draw_lead(lead_one, self._lead_vertices[0], rect)

      if lead_two and lead_two.status and lead_one and (abs(lead_one.dRel - lead_two.dRel) > 3.0):
        self._draw_lead(lead_two, self._lead_vertices[1], rect)

  def _update_raw_points(self, model):
    """Update raw points from model data before transformation"""
    # Update track raw points
    track_points = np.array([model.position.x, model.position.y, model.position.z], dtype=np.float32).T
    self._track.raw_points = track_points

    # Update lane line raw points
    for i, lane_line in enumerate(model.laneLines):
      lane_points = np.array([lane_line.x, lane_line.y, lane_line.z], dtype=np.float32).T
      self._lane_lines[i].raw_points = lane_points

    # Update road edge raw points
    for i, road_edge in enumerate(model.roadEdges):
      edge_points = np.array([road_edge.x, road_edge.y, road_edge.z], dtype=np.float32).T
      self._road_edges[i].raw_points = edge_points

    self._lane_line_probs = np.array(model.laneLineProbs, dtype=np.float32)
    self._road_edge_stds = np.array(model.roadEdgeStds, dtype=np.float32)

  def _update_leads(self, radar_state, pos_x_array):
    """Update positions of lead vehicles"""
    leads = [radar_state.leadOne, radar_state.leadTwo]
    for i, lead_data in enumerate(leads):
      if lead_data and lead_data.status:
        d_rel = lead_data.dRel
        y_rel = lead_data.yRel
        idx = self._get_path_length_idx(pos_x_array, d_rel)
        z = self._track.raw_points[idx, 2] if idx < len(self._track.raw_points) else 0.0
        self._lead_vertices[i] = self._map_to_screen(d_rel, -y_rel, z + self._path_offset_z)

  def _update_model(self, lead, pos_x_array):
    """Update model visualization data based on model message"""
    max_distance = np.clip(pos_x_array[-1], MIN_DRAW_DISTANCE, MAX_DRAW_DISTANCE)
    max_idx = self._get_path_length_idx(pos_x_array, max_distance)

    # Update lane lines using raw points
    for i in range(4):
      self._lane_lines[i].transformed_points = self._map_raw_points_to_polygon(
        self._lane_lines[i].raw_points, 0.025 * self._lane_line_probs[i], 0, max_idx
      )

    # Update road edges using raw points
    for i in range(2):
      self._road_edges[i].transformed_points = self._map_raw_points_to_polygon(
        self._road_edges[i].raw_points, 0.025, 0, max_idx
      )

    # Update path using raw points
    if lead and lead.status:
      lead_d = lead.dRel * 2.0
      max_distance = np.clip(lead_d - min(lead_d * 0.35, 10.0), 0.0, max_distance)
      max_idx = self._get_path_length_idx(pos_x_array, max_distance)

    self._track.transformed_points = self._map_raw_points_to_polygon(
      self._track.raw_points, 0.9, self._path_offset_z, max_idx, False
    )

  def _draw_lane_lines(self):
    """Draw lane lines and road edges"""
    for i, lane_line in enumerate(self._lane_lines):
      # Skip if no vertices
      if lane_line.transformed_points.size == 0:
        continue

      # Draw lane line
      alpha = np.clip(self._lane_line_probs[i], 0.0, 0.7)
      if alpha < 0.1:
        continue
      color = rl.Color(255, 255, 255, int(alpha * 255))
      draw_polygon(lane_line.transformed_points, color)

    for i, road_edge in enumerate(self._road_edges):
      # Skip if no vertices
      if road_edge.transformed_points.size == 0:
        continue

      # Draw road edge
      alpha = np.clip(1.0 - self._road_edge_stds[i], 0.0, 1.0)
      if alpha < 0.1:
        continue
      color = rl.Color(255, 0, 0, int(alpha * 255))
      draw_polygon(road_edge.transformed_points, color)

  def _draw_path(self, sm, model, height):
    """Draw the path polygon with gradient based on acceleration"""
    if self._track.transformed_points.size == 0:
      return

    if self._experimental_mode:
      # Draw with acceleration coloring
      acceleration = model.acceleration.x
      max_len = min(len(self._track.transformed_points) // 2, len(acceleration))

      # Find midpoint index for polygon
      mid_point = len(self._track.transformed_points) // 2

      # For acceleration-based coloring, process segments separately
      left_side = self._track.transformed_points[:mid_point]
      right_side = self._track.transformed_points[mid_point:][::-1]  # Reverse for proper winding

      # Create segments for gradient coloring
      segment_colors = []
      gradient_stops = []

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
        lightness = self._map_val(saturation, 0.0, 1.0, 0.95, 0.62)
        alpha = self._map_val(lin_grad_point, 0.75 / 2.0, 0.75, 0.4, 0.0)

        # Use HSL to RGB conversion
        color = self._hsla_to_color(path_hue / 360.0, saturation, lightness, alpha)

        # Create quad segment
        gradient_stops.append(lin_grad_point)
        segment_colors.append(color)

      if len(segment_colors) < 2:
        draw_polygon(self._track.transformed_points, rl.Color(255, 255, 255, 30))
        return

      # Create gradient specification
      gradient = {
        'start': (0.0, 1.0),  # Bottom of path
        'end': (0.0, 0.0),  # Top of path
        'colors': segment_colors,
        'stops': gradient_stops,
      }
      draw_polygon(self._track.transformed_points, gradient=gradient)
    else:
      # Draw with throttle/no throttle gradient
      allow_throttle = sm['longitudinalPlan'].allowThrottle or not self._longitudinal_control

      # Start transition if throttle state changes
      if allow_throttle != self._prev_allow_throttle:
        self._prev_allow_throttle = allow_throttle
        self._blend_factor = max(1.0 - self._blend_factor, 0.0)

      # Update blend factor
      if self._blend_factor < 1.0:
        self._blend_factor = min(self._blend_factor + 0.1, 1.0)

      begin_colors = NO_THROTTLE_COLORS if allow_throttle else THROTTLE_COLORS
      end_colors = THROTTLE_COLORS if allow_throttle else NO_THROTTLE_COLORS

      # Blend colors based on transition
      colors = [
        self._blend_colors(begin_colors[0], end_colors[0], self._blend_factor),
        self._blend_colors(begin_colors[1], end_colors[1], self._blend_factor),
        self._blend_colors(begin_colors[2], end_colors[2], self._blend_factor),
      ]

      gradient = {
        'start': (0.0, 1.0),  # Bottom of path
        'end': (0.0, 0.0),  # Top of path
        'colors': colors,
        'stops': [0.0, 1.0],
      }
      draw_polygon(self._track.transformed_points, gradient=gradient)

  def _draw_lead(self, lead_data, vd, rect):
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
    rl.draw_triangle_fan(glow, len(glow), rl.Color(218, 202, 37, 255))

    # Draw chevron
    chevron = [(x + (sz * 1.25), y + sz), (x, y), (x - (sz * 1.25), y + sz)]
    rl.draw_triangle_fan(chevron, len(chevron), rl.Color(201, 34, 49, int(fill_alpha)))

  @staticmethod
  def _get_path_length_idx(pos_x_array: np.ndarray, path_height: float) -> int:
    """Get the index corresponding to the given path height"""
    idx = np.searchsorted(pos_x_array, path_height, side='right')
    return int(np.clip(idx - 1, 0, len(pos_x_array) - 1))

  def _map_to_screen(self, in_x, in_y, in_z):
    """Project a point in car space to screen space"""
    input_pt = np.array([in_x, in_y, in_z])
    pt = self._car_space_transform @ input_pt

    if abs(pt[2]) < 1e-6:
      return None

    x = pt[0] / pt[2]
    y = pt[1] / pt[2]

    clip = self._clip_region
    if x < clip.x or x > clip.x + clip.width or y < clip.y or y > clip.y + clip.height:
      return None

    return (x, y)

  def _batch_map_to_screen(self, coords_3d, allow_invert=True):
    """Batch transform 3D coordinates to screen space"""
    if coords_3d.shape[0] == 0:
        return np.empty((0, 2), dtype=np.float32)

    # Add homogeneous coordinate and transform all points at once
    homogeneous = np.column_stack([coords_3d, np.ones(coords_3d.shape[0])])
    transformed = (self._car_space_transform @ homogeneous[:, :3].T).T

    # Filter out points with zero or near-zero z
    valid_z_mask = np.abs(transformed[:, 2]) > 1e-6
    # if not np.any(valid_z_mask):
    #     return np.empty((0, 2), dtype=np.float32)

    valid_transformed = transformed[valid_z_mask]

    # Project to 2D using vectorized division
    screen_coords = valid_transformed[:, :2] / valid_transformed[:, 2:3]

    # Apply clipping bounds
    clip = self._clip_region
    l, t, r, b = clip.x, clip.y, clip.x + clip.width, clip.y + clip.height
    in_bounds_mask = (
      (screen_coords[:, 0] >= l) &
      (screen_coords[:, 0] <= r) &
      (screen_coords[:, 1] >= t) &
      (screen_coords[:, 1] <= b)
    )

    # if not np.any(in_bounds_mask):
    #     return np.empty((0, 2), dtype=np.float32)

    result_coords = screen_coords[in_bounds_mask]

    # Handle inversion check for hill detection
    if not allow_invert and result_coords.shape[0] > 1:
        # Remove points that go backwards in y (over hills)
        y_diff = np.diff(result_coords[:, 1])
        valid_points_mask = np.concatenate([[True], y_diff <= 0])
        result_coords = result_coords[valid_points_mask]

    return result_coords

  def _map_raw_points_to_polygon(self, raw_points, y_off, z_off, max_idx, allow_invert=True) -> np.ndarray:
    """Convert raw 3D points to a 2D polygon for drawing with optimized batch processing."""
    # Handle empty/invalid input
    if len(raw_points) == 0 or max_idx <= 0:
      return np.empty((0, 2), dtype=np.float32)

    # Slice up to max_idx (clamped to array length)
    xyz = raw_points[: min(max_idx + 1, len(raw_points))]

    # Filter out negative x values (behind camera)
    valid = xyz[:, 0] >= 0
    if not valid.any():
      return np.empty((0, 2), dtype=np.float32)
    xyz = xyz[valid]

    # Create left and right coordinate arrays
    left_coords = xyz + [0, -y_off, z_off]
    right_coords = xyz + [0, y_off, z_off]

    # Combine into one large array for a single transformation
    all_coords = np.vstack([left_coords, right_coords])

    # Single batch transform of all points
    homogeneous = np.column_stack([all_coords, np.ones(all_coords.shape[0])])
    transformed = (self._car_space_transform @ homogeneous[:, :3].T).T

    # Calculate screen coordinates for all points at once
    valid_z_mask = np.abs(transformed[:, 2]) > 1e-6
    if not np.any(valid_z_mask):
      return np.empty((0, 2), dtype=np.float32)

    screen_coords = transformed[valid_z_mask]
    screen_coords[:, :2] = screen_coords[:, :2] / screen_coords[:, 2:3]
    screen_coords = screen_coords[:, :2]  # Keep only x, y

    # Apply clipping bounds
    clip = self._clip_region
    l, t, r, b = clip.x, clip.y, clip.x + clip.width, clip.y + clip.height
    in_bounds_mask = (
      (screen_coords[:, 0] >= l) & (screen_coords[:, 0] <= r) & (screen_coords[:, 1] >= t) & (screen_coords[:, 1] <= b)
    )

    screen_coords = screen_coords[in_bounds_mask]

    # Split back into left and right
    n_left = min(left_coords.shape[0], len(screen_coords) // 2)
    left_screen = screen_coords[:n_left]
    right_screen = screen_coords[n_left:]

    # Handle inversion check for hill detection
    if not allow_invert and left_screen.shape[0] > 1:
      # Remove points that go backwards in y (over hills)
      y_diff = np.diff(left_screen[:, 1])
      valid_points_mask = np.concatenate([[True], y_diff <= 0])
      left_screen = left_screen[valid_points_mask]

    if not allow_invert and right_screen.shape[0] > 1:
      # Do the same for right points
      y_diff = np.diff(right_screen[:, 1])
      valid_points_mask = np.concatenate([[True], y_diff <= 0])
      right_screen = right_screen[valid_points_mask]

    # Return empty array if no valid screen coordinates
    if left_screen.size == 0 or right_screen.size == 0:
      return np.empty((0, 2), dtype=np.float32)

    # Combine left and right points (reverse right for proper polygon winding)
    return np.vstack([left_screen, right_screen[::-1]]).astype(np.float32)

  # def _map_line_to_polygon(self, line, y_off, z_off, max_idx, allow_invert=True) -> np.ndarray:
  #   """Convert a 3D line to a 2D polygon for drawing."""
  #   # Convert to 2D numpy array and handle empty/invalid input
  #   xyz = np.array([line.x, line.y, line.z], dtype=np.float32).T
  #   if len(xyz) == 0 or max_idx <= 0:
  #       return np.empty((0, 2), dtype=np.float32)

  #   # Slice up to max_idx (clamped to array length)
  #   xyz = xyz[:min(max_idx + 1, len(xyz))]

  #   # Filter out negative x values (behind camera)
  #   valid = xyz[:, 0] >= 0
  #   if not valid.any():
  #       return np.empty((0, 2), dtype=np.float32)
  #   xyz = xyz[valid]

  #   # Create left and right coordinate arrays
  #   left_coords = xyz + [0, -y_off, z_off]
  #   right_coords = xyz + [0, y_off, z_off]

  #   # Batch transform to screen coordinates
  #   left_screen = self._batch_map_to_screen(left_coords, allow_invert)
  #   right_screen = self._batch_map_to_screen(right_coords, allow_invert)

  #   # Return empty array if no valid screen coordinates
  #   if left_screen.size == 0 or right_screen.size == 0:
  #       return np.empty((0, 2), dtype=np.float32)

  #   # Combine left and right points (reverse right for proper polygon winding)
  #   return np.vstack([left_screen, right_screen[::-1]]).astype(np.float32)


  @staticmethod
  def _map_val(x, x0, x1, y0, y1):
    """Map value x from range [x0, x1] to range [y0, y1]"""
    return y0 + (y1 - y0) * ((x - x0) / (x1 - x0)) if x1 != x0 else y0

  @staticmethod
  def _hsla_to_color(h, s, l, a):
    """Convert HSLA color to Raylib Color using colorsys module"""
    # colorsys uses HLS format (Hue, Lightness, Saturation)
    r, g, b = colorsys.hls_to_rgb(h, l, s)

    # Ensure values are in valid range
    r_val = max(0, min(255, int(r * 255)))
    g_val = max(0, min(255, int(g * 255)))
    b_val = max(0, min(255, int(b * 255)))
    a_val = max(0, min(255, int(a * 255)))

    return rl.Color(r_val, g_val, b_val, a_val)

  @staticmethod
  def _blend_colors(start, end, t):
    """Blend between two colors with factor t"""
    if t >= 1.0:
      return end

    return rl.Color(
      int((1 - t) * start.r + t * end.r),
      int((1 - t) * start.g + t * end.g),
      int((1 - t) * start.b + t * end.b),
      int((1 - t) * start.a + t * end.a),
    )
