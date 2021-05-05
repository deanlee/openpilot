#pragma once

#include "selfdrive/hardware/base.h"

class HardwareTici : public HardwareNone {
 public:
  static constexpr float MAX_VOLUME = 0.5;
  static constexpr float MIN_VOLUME = 0.4;
  static Type type() { return typeTICI; }
  static std::string get_os_version();

  static void reboot() { std::system("sudo reboot"); }
  static void poweroff() { std::system("sudo poweroff"); }
  static void set_brightness(int percent);
  static void set_display_power(bool on) {}

  static bool get_ssh_enabled();
  static void set_ssh_enabled(bool enabled);
};
