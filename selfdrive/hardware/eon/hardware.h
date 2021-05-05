#pragma once

#include "selfdrive/hardware/base.h"

class HardwareEon : public HardwareNone {
public:
  static constexpr float MAX_VOLUME = 1.0;
  static constexpr float MIN_VOLUME = 0.5;

  static Type type() { return typeEON; }
  static std::string get_os_version();

  static void reboot() { std::system("reboot"); };
  static void poweroff() { std::system("LD_LIBRARY_PATH= svc power shutdown"); };
  static void set_brightness(int percent);
  static void set_display_power(bool on);
  static bool get_ssh_enabled();
  static void set_ssh_enabled(bool enabled);
  // android only
  inline static bool launched_activity = false;
  static void check_activity() {
    int ret = std::system("dumpsys SurfaceFlinger --list | grep -Fq 'com.android.settings'");
    launched_activity = ret == 0;
  }

  static void close_activities() {
    if(launched_activity){
      std::system("pm disable com.android.settings && pm enable com.android.settings");
    }
  }

  static void launch_activity(std::string activity, std::string opts = "") {
    if (!launched_activity) {
      std::string cmd = "am start -n " + activity + " " + opts +
                        " --ez extra_prefs_show_button_bar true \
                         --es extra_prefs_set_next_text ''";
      std::system(cmd.c_str());
    }
    launched_activity = true;
  }
  static void launch_wifi() {
    launch_activity("com.android.settings/.wifi.WifiPickerActivity", "-a android.net.wifi.PICK_WIFI_NETWORK");
  }
  static void launch_tethering() {
    launch_activity("com.android.settings/.TetherSettings");
  }
};
