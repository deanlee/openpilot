import pyray as rl
from openpilot.system.ui.lib.text_measure import measure_text_cached
from openpilot.system.ui.lib.application import font_fallback


def is_cjk_char(char: str) -> bool:
  """Check if character is CJK (Chinese, Japanese, Korean)."""
  if not char:
    return False
  code = ord(char)
  return (
    (0x4E00 <= code <= 0x9FFF) or    # CJK Unified Ideographs
    (0x3400 <= code <= 0x4DBF) or    # CJK Unified Ideographs Extension A
    (0x20000 <= code <= 0x2A6DF) or  # CJK Unified Ideographs Extension B
    (0xF900 <= code <= 0xFAFF) or    # CJK Compatibility Ideographs
    (0x2F800 <= code <= 0x2FA1F) or  # CJK Compatibility Ideographs Supplement
    (0x3040 <= code <= 0x309F) or    # Hiragana
    (0x30A0 <= code <= 0x30FF) or    # Katakana
    (0xAC00 <= code <= 0xD7AF)       # Hangul Syllables
  )


def tokenize_mixed_text(text: str) -> list[str]:
  """Tokenize text handling CJK and Latin characters."""
  if not text:
    return []

  tokens: list[str] = []
  current: list[str] = []
  in_latin = False

  for char in text:
    if char.isspace() or is_cjk_char(char):
      if current:
        tokens.append(''.join(current))
        current = []
        in_latin = False
      tokens.append(char)
    else:
      if not in_latin and current:
        tokens.append(''.join(current))
        current = []
      current.append(char)
      in_latin = True

  if current:
    tokens.append(''.join(current))

  return tokens


def _break_long_word(font: rl.Font, word: str, font_size: int, max_width: int) -> list[str]:
  if not word:
    return []

  parts = []
  remaining = word

  while remaining:
    if measure_text_cached(font, remaining, font_size).x <= max_width:
      parts.append(remaining)
      break

    # Binary search for the longest substring that fits
    left, right = 1, len(remaining)
    best_fit = 1

    while left <= right:
      mid = (left + right) // 2
      substring = remaining[:mid]
      width = measure_text_cached(font, substring, font_size).x

      if width <= max_width:
        best_fit = mid
        left = mid + 1
      else:
        right = mid - 1

    # Add the part that fits
    parts.append(remaining[:best_fit])
    remaining = remaining[best_fit:]

  return parts


_cache: dict[int, list[str]] = {}


def wrap_text(font: rl.Font, text: str, font_size: int, max_width: int) -> list[str]:
  """Wrap text to fit within max_width, handling mixed CJK and Latin text."""
  font = font_fallback(font)
  key = hash((font.texture.id, text, font_size, max_width))
  if key in _cache:
    return _cache[key]

  if not text or max_width <= 0:
    return []

  all_lines = []

  for paragraph in text.split('\n'):
    if not paragraph.strip():
      all_lines.append("")
      continue

    tokens = tokenize_mixed_text(paragraph)
    if not tokens:
      all_lines.append("")
      continue

    lines: list[str] = []
    current: list[str] = []

    for token in tokens:
      if not current and token.isspace():
        continue

      token_width = measure_text_cached(font, token, font_size).x

      # Token too long: break it
      if token_width > max_width and not token.isspace():
        if current:
          lines.append(''.join(current).rstrip())
          current = []

        parts = _break_long_word(font, token, font_size, max_width)
        lines.extend(parts[:-1])
        current = [parts[-1]] if parts else []
        continue

      # Test if token fits on current line
      test_line = ''.join(current + [token])
      test_width = measure_text_cached(font, test_line, font_size).x

      if test_width <= max_width:
        current.append(token)
      else:
        if current:
          lines.append(''.join(current).rstrip())
        current = [token] if not token.isspace() else []

    if current:
      lines.append(''.join(current).rstrip())

    all_lines.extend(lines)

  _cache[key] = all_lines
  return all_lines
