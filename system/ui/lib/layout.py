from dataclasses import dataclass, field
from enum import Enum, auto
from typing import List, Optional, Union, Tuple
import pyray as rl


class Alignment(Enum):
  START = auto()
  CENTER = auto()
  END = auto()


@dataclass
class Spacing:
  left: float = 0
  top: float = 0
  right: float = 0
  bottom: float = 0


class LayoutItem:
  def __init__(self):
    self.rect = rl.Rectangle(0, 0, 0, 0)
    self.min_size: Tuple[float, float] = (0, 0)
    self.max_size: Tuple[float, float] = (float('inf'), float('inf'))
    self.fixed_size: Optional[Tuple[float, float]] = None
    self.stretch: float = 1.0
    self.margin = Spacing()
    self.alignment = (Alignment.START, Alignment.CENTER)  # (horizontal, vertical)
    self.visible: bool = True

  def set_visible(self, visible: bool):
    self.visible = visible

  def set_geometry(self, rect: rl.Rectangle):
    self.rect = rect

  def minimum_size(self) -> Tuple[float, float]:
    if self.fixed_size:
      return self.fixed_size
    return self.min_size

  def maximum_size(self) -> Tuple[float, float]:
    if self.fixed_size:
      return self.fixed_size
    return self.max_size

  def size_hint(self) -> Tuple[float, float]:
    if self.fixed_size:
      return self.fixed_size
    return self.min_size


class Layout(LayoutItem):
  def __init__(self):
    super().__init__()
    self.items: List[LayoutItem] = []
    self.spacing: float = 0
    self.padding = Spacing()

  def add_item(self, item: LayoutItem) -> LayoutItem:
    self.items.append(item)
    return item

  def add_layout(self) -> 'Layout':
    layout = type(self)()
    self.add_item(layout)
    return layout

  def add_fixed_item(self, width: float, height: float) -> LayoutItem:
    item = LayoutItem()
    item.fixed_size = (width, height)
    return self.add_item(item)

  def add_stretch(self, stretch: float = 1.0) -> LayoutItem:
    item = LayoutItem()
    item.stretch = stretch
    return self.add_item(item)

  def update_layout(self):
    raise NotImplementedError


class HLayout(Layout):
  def update_layout(self):
    visible_items = [item for item in self.items if getattr(item, "visible", True)]
    if not visible_items:
      return

    # Calculate available space
    content_width = self.rect.width - self.padding.left - self.padding.right - self.spacing * (len(visible_items) - 1)
    content_height = self.rect.height - self.padding.top - self.padding.bottom

    # Calculate stretch units
    total_fixed_width = sum(
      item.fixed_size[0] + item.margin.left + item.margin.right for item in visible_items if item.fixed_size
    )
    total_stretch = sum(item.stretch for item in visible_items if not item.fixed_size)

    stretch_unit = max(0, (content_width - total_fixed_width) / total_stretch) if total_stretch else 0

    # Position items
    x = self.rect.x + self.padding.left
    for item in visible_items:
      # Calculate item width
      if item.fixed_size:
        width = item.fixed_size[0]
        height = item.fixed_size[1]
      else:
        width = stretch_unit * item.stretch
        height = item.size_hint()[1]

      # Apply margins
      x += item.margin.left
      effective_height = content_height - item.margin.top - item.margin.bottom

      # Calculate vertical position based on alignment
      y = self.rect.y + self.padding.top + item.margin.top
      if item.alignment[1] == Alignment.CENTER:
        y += (effective_height - height) / 2
        print(y)
      elif item.alignment[1] == Alignment.END:
        y += effective_height - height

      # Set item geometry
      item.set_geometry(rl.Rectangle(x, y, width, height))

      # Update position for next item
      x += width + item.margin.right + self.spacing

      # Update nested layouts
      if isinstance(item, Layout):
        item.update_layout()


class VLayout(Layout):
  def update_layout(self):
    visible_items = [item for item in self.items if getattr(item, "visible", True)]
    if not visible_items:
      return

    # Calculate available space
    content_width = self.rect.width - self.padding.left - self.padding.right
    content_height = self.rect.height - self.padding.top - self.padding.bottom - self.spacing * (len(visible_items) - 1)

    # Calculate stretch units
    total_fixed_height = sum(
      item.fixed_size[1] + item.margin.top + item.margin.bottom for item in visible_items if item.fixed_size
    )
    total_stretch = sum(item.stretch for item in visible_items if not item.fixed_size)

    stretch_unit = max(0, (content_height - total_fixed_height) / total_stretch) if total_stretch else 0

    # Position items
    y = self.rect.y + self.padding.top
    for item in visible_items:
      # Calculate item height
      if item.fixed_size:
        height = item.fixed_size[1]
      else:
        height = stretch_unit * item.stretch

      # Apply margins
      y += item.margin.top
      effective_width = content_width - item.margin.left - item.margin.right

      # Calculate horizontal position based on alignment
      x = self.rect.x + self.padding.left + item.margin.left
      if item.alignment[0] == Alignment.CENTER:
        x += (effective_width - item.size_hint()[0]) / 2
      elif item.alignment[0] == Alignment.END:
        x += effective_width - item.size_hint()[0]

      # Set item geometry
      item.set_geometry(rl.Rectangle(x, y, effective_width, height))

      # Update position for next item
      y += height + item.margin.bottom + self.spacing

      # Update nested layouts
      if isinstance(item, Layout):
        item.update_layout()
