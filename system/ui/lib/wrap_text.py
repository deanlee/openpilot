import pyray as rl

_cache: dict[int, list[str]] = {}


def wrap_text(font: rl.Font, text: str, font_size: int, max_width: int, spacing: int = 0) -> list[str]:
  if not text or max_width <= 0:
    return []

  key = hash((font.texture.id, text, font_size, max_width, spacing))
  if key in _cache:
    return _cache[key]

  lines: list[str] = []
  current_line: list[str] = []
  current_width: int = 0
  space_width = rl.measure_text_ex(font, " ", font_size, 0).x  # noqa: TID251

  for paragraph in text.split('\n'):
    if not paragraph:
      lines.append("")
      continue

    for word in paragraph.split():
      word_width = int(rl.measure_text_ex(font, word, font_size, 0).x)  # noqa: TID251

      if word_width > max_width:
        if current_line:
          lines.append(" ".join(current_line))
          current_line, current_width = [], 0
        # Break the word into parts that fit
        while word:
          for i in range(len(word), 0, -1):
            if rl.measure_text_ex(font, word[:i], font_size, 0).x <= max_width:  # noqa: TID251
              lines.append(word[:i])
              word = word[i:]
              break
          else:
            lines.append(word[:1])
            word = word[1:]
        continue

      needed_width = int(current_width + (space_width if current_line else 0) + word_width)
      if needed_width <= max_width:
        current_line.append(word)
        current_width = needed_width
      else:
        if current_line:
          lines.append(" ".join(current_line))
        current_line, current_width = [word], word_width

    if current_line:
      lines.append(" ".join(current_line))
      current_line, current_width = [], 0

  _cache[key] = lines
  return lines
