import pyray as rl
from openpilot.system.ui.lib.text_measure import measure_text_cached

# ---------------------------------------------------------------------
# CJK detection
# ---------------------------------------------------------------------


def is_cjk(ch: str) -> bool:
  o = ord(ch)
  return (
    0x4E00 <= o <= 0x9FFF  # CJK Unified Ideographs
    or 0x3400 <= o <= 0x4DBF  # CJK Extension A
    or 0x3040 <= o <= 0x30FF  # Hiragana / Katakana
    or 0xAC00 <= o <= 0xD7AF  # Hangul
  )


# ---------------------------------------------------------------------
# Tokenizer
#   - English / numbers → word token
#   - CJK → single char token
#   - Spaces preserved
# ---------------------------------------------------------------------


def tokenize(text: str) -> list[str]:
  tokens: list[str] = []
  buf: list[str] = []

  for ch in text:
    if ch.isspace():
      if buf:
        tokens.append("".join(buf))
        buf.clear()
      tokens.append(ch)
    elif is_cjk(ch):
      if buf:
        tokens.append("".join(buf))
        buf.clear()
      tokens.append(ch)
    else:
      buf.append(ch)

  if buf:
    tokens.append("".join(buf))

  return tokens


# ---------------------------------------------------------------------
# Cache
# ---------------------------------------------------------------------

_cache: dict[tuple[int, str, int, int, float], list[str]] = {}


# ---------------------------------------------------------------------
# Main API
# ---------------------------------------------------------------------


def wrap_text(font: rl.Font, text: str, font_size: int, max_width: int, spacing: float = 0.0, emojis: bool = False) -> list[str]:
  if not text or max_width <= 0:
    return []

  spacing = round(spacing, 4)
  key = (font.texture.id, text, font_size, max_width, spacing)
  if key in _cache:
    return _cache[key]

  measure = measure_text_cached
  space_width = measure(font, " ", font_size, spacing, emojis).x

  lines: list[str] = []

  for paragraph in text.split("\n"):
    tokens = tokenize(paragraph)

    cur_line: list[str] = []
    cur_width = 0.0

    i = 0
    while i < len(tokens):
      tok = tokens[i]

      # Explicit spaces
      if tok == " ":
        if cur_line:
          cur_line.append(tok)
          cur_width += space_width
        i += 1
        continue

      tok_width = measure(font, tok, font_size, spacing, emojis).x

      # Token too wide → binary split
      if tok_width > max_width:
        if cur_line:
          lines.append("".join(cur_line).rstrip())
          cur_line = []
          cur_width = 0.0

        left, right = 1, len(tok)
        best = 1
        while left <= right:
          mid = (left + right) // 2
          if measure(font, tok[:mid], font_size, spacing, emojis).x <= max_width:
            best = mid
            left = mid + 1
          else:
            right = mid - 1

        lines.append(tok[:best])
        rest = tok[best:]
        if rest:
          tokens.insert(i + 1, rest)
        i += 1
        continue

      # Normal append
      if cur_width + tok_width <= max_width:
        cur_line.append(tok)
        cur_width += tok_width
      else:
        lines.append("".join(cur_line).rstrip())
        cur_line = [tok]
        cur_width = tok_width

      i += 1

    if cur_line:
      lines.append("".join(cur_line).rstrip())

  _cache[key] = lines
  return lines
