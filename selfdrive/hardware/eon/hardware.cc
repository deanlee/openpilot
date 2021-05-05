#include "selfdrive/hardware/eon/hardware.h"

#include <gui/ISurfaceComposer.h>
#include <gui/SurfaceComposerClient.h>
#include <hardware/hwcomposer_defs.h>

#include <cstdlib>
#include <fstream>

#include "selfdrive/common/util.h"

std::string HardwareEon::get_os_version() {
  return "NEOS " + util::read_file("/VERSION");
};

void HardwareEon::set_brightness(int percent) {
    std::ofstream brightness_control("/sys/class/leds/lcd-backlight/brightness");
    if (brightness_control.is_open()) {
      brightness_control << (int)(percent * (255/100.)) << "\n";
      brightness_control.close();
    }
  }

    void HardwareEon::set_display_power(bool on) {
    auto dtoken = android::SurfaceComposerClient::getBuiltInDisplay(android::ISurfaceComposer::eDisplayIdMain);
    android::SurfaceComposerClient::setDisplayPowerMode(dtoken, on ? HWC_POWER_MODE_NORMAL : HWC_POWER_MODE_OFF);
  }

   bool HardwareEon::get_ssh_enabled() {
    return std::system("getprop persist.neos.ssh | grep -qF '1'") == 0;
  };
   void HardwareEon::set_ssh_enabled(bool enabled) {
    std::string cmd = util::string_format("setprop persist.neos.ssh %d", enabled ? 1 : 0);
    std::system(cmd.c_str());
  };
