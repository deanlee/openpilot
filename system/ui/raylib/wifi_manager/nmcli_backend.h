#pragma once

#include <map>
#include <string>
#include <tuple>
#include <vector>

enum class SecurityType {
  OPEN,
  WPA,
  UNSUPPORTED
};

struct Network {
  std::string ssid;            // Network name (SSID)
  std::string bssid;           // Access point's MAC address (BSSID)
  bool connected;              // Is the network currently in use
  int strength;                // Signal strength
  SecurityType security_type;  // Type of security used by the network
};

inline bool operator<(const Network& lhs, const Network& rhs) {
  return std::tie(rhs.connected, rhs.strength, rhs.ssid) < std::tie(lhs.connected, lhs.strength, lhs.ssid);
}

namespace wifi {

std::vector<Network> scan_networks();
std::map<std::string, std::string> saved_networks();
bool connect(const std::string& bssid, const std::string& password = "");
bool forget(const std::string& uuid);

}  // namespace wifi
