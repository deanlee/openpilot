#include "system/ui/raylib/wifi_manager/nmcli_backend.h"

#include "common/util.h"

std::string replace_all(std::string str, const std::string& from, const std::string& to) {
  size_t start_pos = 0;
  while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
    str.replace(start_pos, from.length(), to);
    start_pos += to.length();
  }
  return str;
}

std::vector<std::string> split(std::string_view source, char delimiter) {
  std::vector<std::string> fields;
  size_t last = 0;
  for (size_t i = 0; i < source.length(); ++i) {
    if (source[i] == delimiter && source[i - 1] != '\\') {
      fields.emplace_back(source.substr(last, i - last));
      last = i + 1;
    }
  }
  fields.emplace_back(source.substr(last));
  return fields;
}

SecurityType getSecurityType(const std::string& security) {
  if (security.empty() || security == "--") {
    return SecurityType::OPEN;
  } else if (security.find("WPA") != std::string::npos || security.find("RSN") != std::string::npos) {
    return SecurityType::WPA;
  }
  return SecurityType::UNSUPPORTED;
}

namespace wifi {

std::vector<Network> scan_networks() {
  std::vector<Network> networks;

  std::string command = "nmcli -t -c no -f SSID,BSSID,IN-USE,SIGNAL,SECURITY device wifi list";
  for (const auto& line : split(util::check_output(command), '\n')) {
    auto fields = split(line, ':');
    if (fields.size() == 5 && !fields[0].empty()) {
      networks.emplace_back(Network{fields[0], replace_all(fields[1], "\\:", ":"),
                                    fields[2] == "*", std::stoi(fields[3]), getSecurityType(fields[4])});
    }
  }

  std::sort(networks.begin(), networks.end());
  return networks;
}

std::map<std::string, std::string> saved_networks() {
  std::map<std::string, std::string> network_ssids;

  // Get UUIDs of all saved wireless connections
  std::string uuids;
  std::vector<std::string> uuid_vector;
  std::string cmd = "nmcli -t -f UUID,TYPE connection show | grep 802-11-wireless";
  for (auto& line : split(util::check_output(cmd), '\n')) {
    auto connection_info = split(line, ':');
    if (connection_info.size() >= 2) {
      uuid_vector.push_back(connection_info[0]);
      uuids += connection_info[0] + " ";
    }
  }

  // Get SSIDs for the saved connections
  std::string ssid_cmd = "nmcli -t -f 802-11-wireless.ssid connection show " + uuids;
  int index = 0;
  for (const auto& ssd_line : split(util::check_output(ssid_cmd), '\n')) {
    if (!ssd_line.empty()) {
      std::string ssid = split(ssd_line, ':')[1];
      network_ssids[ssid] = uuid_vector[index++];
    }
  }
  return network_ssids;
}

bool connect(const std::string& bssid, const std::string& password) {
  std::string command = "nmcli device wifi connect '" + bssid + "'";
  if (!password.empty()) {
    command += " password '" + password + "'";
  }
  return system(command.c_str()) == 0;
}

bool forget(const std::string& uuid) {
  std::string command = "nmcli connection delete uuid '" + uuid + "'";
  return system(command.c_str()) == 0;
}

}  // namespace wifi
