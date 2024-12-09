def strip_deprecated_keys(d):
  # Use dictionary comprehension to build a new dict, excluding deprecated keys
  return {
    k: strip_deprecated_keys(v) if isinstance(v, dict) else v
    for k, v in d.items() if not k.endswith('DEPRECATED')
  }