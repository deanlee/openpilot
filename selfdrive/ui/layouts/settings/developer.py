from openpilot.common.params import Params
from openpilot.selfdrive.ui.widgets.ssh_key import ssh_key_item
from openpilot.selfdrive.ui.ui_state import ui_state
from openpilot.system.ui.widgets import Widget
from openpilot.system.ui.widgets.list_view import toggle_item
from openpilot.system.ui.widgets.scroller_tici import Scroller
from openpilot.system.ui.widgets.confirm_dialog import ConfirmDialog
from openpilot.system.ui.lib.application import gui_app
from openpilot.system.ui.lib.multilang import tr, tr_lazy, tr_noop
from openpilot.system.ui.widgets import DialogResult

# Description constants
DESCRIPTIONS = {
  'AdbEnabled': tr_noop(
    "ADB (Android Debug Bridge) allows connecting to your device over USB or over the network. " +
    "See https://docs.comma.ai/how-to/connect-to-comma for more info."
  ),
  'ssh_key': tr_noop(
    "Warning: This grants SSH access to all public keys in your GitHub settings. Never enter a GitHub username " +
    "other than your own. A comma employee will NEVER ask you to add their GitHub username."
  ),
  'AlphaLongitudinalEnabled': tr_noop(
    "<b>WARNING: openpilot longitudinal control is in alpha for this car and will disable Automatic Emergency Braking (AEB).</b><br><br>" +
    "On this car, openpilot defaults to the car's built-in ACC instead of openpilot's longitudinal control. " +
    "Enable this to switch to openpilot longitudinal control. Enabling Experimental mode is recommended when enabling openpilot longitudinal control alpha. " +
    "Changing this setting will restart openpilot if the car is powered on."
  ),
}


class DeveloperLayout(Widget):
  def __init__(self):
    super().__init__()
    self._params = Params()
    self._is_release = self._params.get_bool("IsReleaseBranch")
    # Toggle definitions: param, title, callback, enabled
    toogle_defs = {
      "AdbEnabled": (tr_lazy("Enable ADB"), self._on_enable_adb, ui_state.is_offroad),
      "SshEnabled": (tr_lazy("Enable SSH"), self._on_enable_ssh, True),
      "JoystickDebugMode": (tr_lazy("Joystick Debug Mode"), self._on_joystick_debug_mode, ui_state.is_offroad),
      "LongitudinalManeuverMode": (tr_lazy("Longitudinal Maneuver Mode"), self._on_long_maneuver_mode, True),
      "AlphaLongitudinalEnabled": (tr_lazy("openpilot Longitudinal Control (Alpha)"), self._on_alpha_long_enabled, lambda: not ui_state.engaged),
      "ShowDebugInfo": (tr_lazy("UI Debug Mode"), self._on_enable_ui_debug, True),
    }

    self._toggles = {}
    for param, (title, callback, enabled) in toogle_defs.items():
      toggle = toggle_item(title, DESCRIPTIONS.get(param, ""), self._params.get_bool(param), callback=callback, enabled=enabled)
      self._toggles[param] = toggle

    self._on_enable_ui_debug(self._params.get_bool("ShowDebugInfo"))

    widgets = list(self._toggles.values())
    widgets.insert(2, ssh_key_item(tr_lazy("SSH Keys"), description=tr_lazy(DESCRIPTIONS["ssh_key"])))
    self._scroller = Scroller(widgets, line_separator=True, spacing=0)

    # Toggles should be not available to change in onroad state
    ui_state.add_offroad_transition_callback(self._update_toggles)

  def _render(self, rect):
    self._scroller.render(rect)

  def show_event(self):
    self._scroller.show_event()
    self._update_toggles()

  def _update_toggles(self):
    ui_state.update_params()

    # Hide non-release toggles on release builds
    # TODO: we can do an onroad cycle, but alpha long toggle requires a deinit function to re-enable radar and not fault
    for param in ["JoystickDebugMode", "LongitudinalManeuverMode", "AlphaLongitudinalEnabled"]:
      self._toggles[param].set_visible(not self._is_release)

    alpha_avail = ui_state.CP.alphaLongitudinalAvailable if ui_state.CP else False
    if not alpha_avail or self._is_release:
      self._toggles["AlphaLongitudinalEnabled"].set_visible(False)
      if not alpha_avail:
        self._params.remove("AlphaLongitudinalEnabled")

    maneuver_ok = ui_state.has_longitudinal_control and ui_state.is_offroad()
    self._toggles["LongitudinalManeuverMode"].action_item.set_enabled(maneuver_ok)
    if not maneuver_ok and self._params.get_bool("LongitudinalManeuverMode"):
      self._params.put_bool("LongitudinalManeuverMode", False)

    # refresh toggles from params to mirror external changes
    for key, item in self._toggles.items():
      item.action_item.set_state(self._params.get_bool(key))

  def _on_enable_ui_debug(self, state: bool):
    self._params.put_bool("ShowDebugInfo", state)
    gui_app.set_show_touches(state)
    gui_app.set_show_fps(state)

  def _on_enable_adb(self, state: bool):
    self._params.put_bool("AdbEnabled", state)

  def _on_enable_ssh(self, state: bool):
    self._params.put_bool("SshEnabled", state)

  def _on_joystick_debug_mode(self, state: bool):
    self._params.put_bool("JoystickDebugMode", state)
    self._params.put_bool("LongitudinalManeuverMode", False)
    self._toggles["LongitudinalManeuverMode"].action_item.set_state(False)

  def _on_long_maneuver_mode(self, state: bool):
    self._params.put_bool("LongitudinalManeuverMode", state)
    self._params.put_bool("JoystickDebugMode", False)
    self._toggles["JoystickDebugMode"].action_item.set_state(False)

  def _on_alpha_long_enabled(self, state: bool):
    if state:
      toggle = self._toggles["AlphaLongitudinalEnabled"]
      def confirm_callback(result: int):
        if result == DialogResult.CONFIRM:
          self._params.put_bool("AlphaLongitudinalEnabled", True)
          self._params.put_bool("OnroadCycleRequested", True)
          self._update_toggles()
        else:
          toggle.action_item.set_state(False)

      # show confirmation dialog
      content = (f"<h1>{toggle.title}</h1><br>" +
                 f"<p>{toggle.description}</p>")

      dlg = ConfirmDialog(content, tr("Enable"), rich=True)
      gui_app.push_modal_overlay(dlg, callback=confirm_callback)

    else:
      self._params.put_bool("AlphaLongitudinalEnabled", False)
      self._params.put_bool("OnroadCycleRequested", True)
      self._update_toggles()
