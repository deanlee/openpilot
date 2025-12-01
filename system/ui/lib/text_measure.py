import pyray as rl
from openpilot.system.ui.lib.application import FONT_SCALE, font_fallback
from openpilot.system.ui.lib.emoji import parse_text_with_emoji

_cache: dict[int, rl.Vector2] = {}


def measure_text_cached(font: rl.Font, text: str, font_size: int, spacing: float = 0) -> rl.Vector2:
  """Caches text measurements to avoid redundant calculations."""
  font = font_fallback(font)
  spacing = round(spacing, 4)
  key = hash((font.texture.id, text, font_size, spacing))
  if key in _cache:
    return _cache[key]

  # Measure normal characters without emojis, then add standard width for each found emoji
  segments = parse_text_with_emoji(text)
  result = rl.Vector2(0, 0)
  for seg in segments:
    if seg.content:
      size = rl.measure_text_ex(font, seg.content, font_size * FONT_SCALE, spacing)  # noqa: TID251
    elif seg.texture:
      size = rl.Vector2(font_size * FONT_SCALE, font_size * FONT_SCALE)
    result.x += size.x
    result.y = max(result.y, font_size * FONT_SCALE)

  _cache[key] = result
  return result
