class ToggleSettings:
  def __init__(self):
    self.settings = {
      "enable_feature_x": True,
      "enable_feature_y": False,
      "enable_feature_z": True,
    }

  def toggle_setting(self, setting_name: str, value: bool):
    if setting_name in self.settings:
      self.settings[setting_name] = value
    else:
      raise ValueError(f"Setting '{setting_name}' does not exist.")

  def get_setting(self, setting_name: str) -> bool:
    return self.settings.get(setting_name, None)
