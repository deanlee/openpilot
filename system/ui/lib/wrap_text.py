import pyray as rl
from openpilot.system.ui.lib.text_measure import measure_text_cached


def _break_long_word(font: rl.Font, word: str, font_size: int, max_width: int) -> list[str]:
  """Break a word that's too long to fit on a single line."""
  parts = []
  remaining = word

  while remaining:
    # Binary search to find largest substring that fits
    left, right = 0, len(remaining)

    while left < right:
      mid = (left + right + 1) // 2
      if rl.measure_text_ex(font, remaining[:mid], font_size, 0).x <= max_width: # noqa: TID251
        left = mid
      else:
        right = mid - 1

    # Add part that fits and continue with remainder
    parts.append(remaining[:left])
    remaining = remaining[left:]

    # Handle case where not even a single character fits
    if left == 0 and remaining:
      parts.append(remaining[0])
      remaining = remaining[1:]

  return parts


def wrap_text(font: rl.Font, text: str, font_size: int, max_width: int) -> list[str]:
  """Wrap text to fit within the specified width."""
  if not text or max_width <= 0:
    return []

  # Split by newlines to preserve explicit line breaks
  paragraphs = text.split('\n')
  result = []
  space_width = measure_text_cached(font, " ", font_size).x

  for paragraph in paragraphs:
    # Handle empty paragraphs
    if not paragraph:
      result.append("")
      continue

    words = paragraph.split()
    if not words:
      result.append("")
      continue

    current_line = []
    current_width = 0

    for word in words:
      word_width = measure_text_cached(font, word, font_size).x

      # If word is too long for a line, break it and start new line
      if word_width > max_width:
        if current_line:
          result.append(" ".join(current_line))
          current_line = []
        result.extend(_break_long_word(font, word, font_size, max_width))
        current_width = 0
        continue

      # Calculate width with this word
      line_width = current_width + word_width
      if current_line:  # Add space width if not first word
        line_width += space_width

      # Add to current line or start new line
      if line_width <= max_width or not current_line:
        current_line.append(word)
        current_width = line_width
      else:
        result.append(" ".join(current_line))
        current_line = [word]
        current_width = word_width

    # Add any remaining content
    if current_line:
      result.append(" ".join(current_line))

  return result
