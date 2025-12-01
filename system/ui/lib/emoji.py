import io
import re

from dataclasses import dataclass
from PIL import Image, ImageDraw, ImageFont
import pyray as rl

from openpilot.system.ui.lib.application import FONT_DIR

_emoji_font: ImageFont.FreeTypeFont | None = None
_cache: dict[str, rl.Texture] = {}

EMOJI_REGEX = re.compile(
"""[\U0001F600-\U0001F64F
\U0001F300-\U0001F5FF
\U0001F680-\U0001F6FF
\U0001F1E0-\U0001F1FF
\U00002700-\U000027BF
\U0001F900-\U0001F9FF
\U00002600-\U000026FF
\U00002300-\U000023FF
\U00002B00-\U00002BFF
\U0001FA70-\U0001FAFF
\U0001F700-\U0001F77F
\u2640-\u2642
\u2600-\u2B55
\u200d
\u23cf
\u23e9
\u231a
\ufe0f
\u3030
]+""".replace("\n", ""),
  flags=re.UNICODE
)

@dataclass(frozen=True, slots=True)
class TextSegment:
  content: str = ""
  texture: rl.Texture | None = None  # Only set for emoji segments


def _load_emoji_font() -> ImageFont.FreeTypeFont | None:
  global _emoji_font
  if _emoji_font is None:
    _emoji_font = ImageFont.truetype(str(FONT_DIR.joinpath("NotoColorEmoji.ttf")), 109)
  return _emoji_font

def find_emoji(text):
  return [(m.start(), m.end(), m.group()) for m in EMOJI_REGEX.finditer(text)]

def parse_text_with_emoji(text: str) -> list[TextSegment]:
  emoji_matches = find_emoji(text)

  if not emoji_matches:
    return [TextSegment(content=text, start=0, end=len(text), texture=None)]

  segments = []
  prev_end = 0

  for match in emoji_matches:
    # Add text before emoji
    if match.start > prev_end:
      segments.append(TextSegment(content=text[prev_end:match.start], texture=None))

    # Add emoji
    segments.append(TextSegment(content="", texture=emoji_tex(match.emoji)))

    prev_end = match.end

  # Add remaining text
  if prev_end < len(text):
    segments.append(TextSegment(content=text[prev_end:], texture=None))

  return tuple(segments)

def emoji_tex(emoji):
  if emoji not in _cache:
    img = Image.new("RGBA", (128, 128), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    draw.text((0, 0), emoji, font=_load_emoji_font(), embedded_color=True)
    with io.BytesIO() as buffer:
      img.save(buffer, format="PNG")
      l = buffer.tell()
      buffer.seek(0)
      _cache[emoji] = rl.load_texture_from_image(rl.load_image_from_memory(".png", buffer.getvalue(), l))
  return _cache[emoji]
