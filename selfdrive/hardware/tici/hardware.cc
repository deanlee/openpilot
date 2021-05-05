#include "selfdrive/hardware/hardware.h"

#include <cstdlib>
#include <fstream>

#include "selfdrive/common/params.h"
#include "selfdrive/common/util.h"

std::string get_os_version::get_os_version() {
  return "AGNOS " + util::read_file("/VERSION");
};

void HardwareTici::set_brightness(int percent) {
  std::ofstream brightness_control("/sys/class/backlight/panel0-backlight/brightness");
  if (brightness_control.is_open()) {
    brightness_control << (percent * (int)(1023 / 100.)) << "\n";
    brightness_control.close();
  }
};

bool HardwareTici::get_ssh_enabled() { return Params().getBool("SshEnabled"); };
void HardwareTici::set_ssh_enabled(bool enabled) { Params().putBool("SshEnabled", enabled); };