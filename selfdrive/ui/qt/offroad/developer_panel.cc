#include "selfdrive/ui/qt/offroad/developer_panel.h"
#include "selfdrive/ui/qt/widgets/ssh_keys.h"
#include "selfdrive/ui/qt/widgets/controls.h"

DeveloperPanel::DeveloperPanel(SettingsWindow *parent) : ListWidget(parent) {
  adbToggle = new ParamControl("AdbEnabled", tr("Enable ADB"),
            tr("ADB (Android Debug Bridge) allows connecting to your device over USB or over the network. See https://docs.comma.ai/how-to/connect-to-comma for more info."), "");
  addItem(adbToggle);

  addItem(new SshToggle());
  addItem(new SshControl());

  is_release = params.getBool("IsReleaseBranch");
  if (!is_release) {
    initDeveloperToggles();
  }
}

void DeveloperPanel::initDeveloperToggles() {
  joystickToggle = new ParamControl("JoystickDebugMode", tr("Joystick Debug Mode"), "", "");
  QObject::connect(joystickToggle, &ParamControl::toggleFlipped, [=](bool state) {
    params.putBool("LongitudinalManeuverMode", false);
    longManeuverToggle->refresh();
  });
  addItem(joystickToggle);

  longManeuverToggle = new ParamControl("LongitudinalManeuverMode", tr("Longitudinal Maneuver Mode"), "", "");
  QObject::connect(longManeuverToggle, &ParamControl::toggleFlipped, [=](bool state) {
    params.putBool("JoystickDebugMode", false);
    joystickToggle->refresh();
  });
  addItem(longManeuverToggle);

  experimentalLongitudinalToggle = new ParamControl(
    "ExperimentalLongitudinalEnabled",
    tr("openpilot Longitudinal Control (Alpha)"),
    QString("<b>%1</b><br><br>%2")
      .arg(tr("WARNING: openpilot longitudinal control is in alpha for this car and will disable Automatic Emergency Braking (AEB)."))
      .arg(tr("On this car, openpilot defaults to the car's built-in ACC instead of openpilot's longitudinal control. "
              "Enable this to switch to openpilot longitudinal control. Enabling Experimental mode is recommended when enabling openpilot longitudinal control alpha.")),
    ""
  );
  experimentalLongitudinalToggle->setConfirmation(true, false);
  QObject::connect(experimentalLongitudinalToggle, &ParamControl::toggleFlipped, [=]() {
    updateToggles(offroad);
  });
  addItem(experimentalLongitudinalToggle);

  QObject::connect(uiState(), &UIState::offroadTransition, this, &DeveloperPanel::updateToggles);
}

void DeveloperPanel::updateToggles(bool _offroad) {
  offroad = _offroad;

  auto cp_bytes = params.get("CarParamsPersistent");
  bool has_exp_long = false, has_long_control = false;
  if (!cp_bytes.empty()) {
    AlignedBuffer aligned_buf;
    capnp::FlatArrayMessageReader cmsg(aligned_buf.align(cp_bytes.data(), cp_bytes.size()));
    cereal::CarParams::Reader CP = cmsg.getRoot<cereal::CarParams>();
    has_exp_long = CP.getExperimentalLongitudinalAvailable();
    has_long_control = hasLongitudinalControl(CP);
  }
  if (!has_exp_long) {
    params.remove("ExperimentalLongitudinalEnabled");
  }

  experimentalLongitudinalToggle->setVisible(has_exp_long);
  experimentalLongitudinalToggle->refresh();
  longManeuverToggle->setEnabled(has_long_control && _offroad);
  joystickToggle->setEnabled(_offroad);
}

void DeveloperPanel::showEvent(QShowEvent *event) {
  if (!is_release) updateToggles(offroad);
}
