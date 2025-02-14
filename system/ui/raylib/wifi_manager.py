import pyray as rl
import dbus

class WifiManager:
  def __init__(self):
    self.networks = []
    self.connected_network = None
    self.bus = dbus.SystemBus()
    self.network_manager = self.bus.get_object('org.freedesktop.NetworkManager',
                                               '/org/freedesktop/NetworkManager')
    self.network_manager_interface = dbus.Interface(self.network_manager,
                                                    'org.freedesktop.NetworkManager')

  def get_available_networks(self):
    """Retrieve available WiFi networks."""
    devices = self.network_manager_interface.GetDevices()
    for device_path in devices:
      device = self.bus.get_object('org.freedesktop.NetworkManager', device_path)

      # Use the org.freedesktop.DBus.Properties interface to get the DeviceType property
      device_properties = dbus.Interface(device, 'org.freedesktop.DBus.Properties')
      device_type = device_properties.Get('org.freedesktop.NetworkManager.Device', 'DeviceType')

      print(f"Device Type: {device_type}")

      # Type 2 indicates WiFi device
      if device_type == 2:
        wifi_device = dbus.Interface(device, 'org.freedesktop.NetworkManager.Device.Wireless')
        access_points = wifi_device.GetAccessPoints()

        self.networks = []
        for ap_path in access_points:
          ap = self.bus.get_object('org.freedesktop.NetworkManager', ap_path)

          # Get the SSID using org.freedesktop.DBus.Properties
          ap_properties = dbus.Interface(ap, 'org.freedesktop.DBus.Properties')
          ssid = ap_properties.Get('org.freedesktop.NetworkManager.AccessPoint', 'Ssid')
          ssid_str = ''.join([chr(b) for b in ssid])  # Convert to string

          self.networks.append(ssid_str)

  def connect_to_network(self, ssid, password):
    """Connect to the selected WiFi network."""
    settings = {
      '802-11-wireless': {'ssid': dbus.ByteArray(ssid.encode('utf8')),
                          'security': '802-11-wireless-security'},
      '802-11-wireless-security': {'key-mgmt': 'wpa-psk', 'psk': password},
      'ipv4': {'method': 'auto'},
      'ipv6': {'method': 'ignore'}
    }

    connection = self.network_manager_interface.AddAndActivateConnection(
      settings, "/org/freedesktop/NetworkManager/Devices/0", "/"
    )
    self.connected_network = ssid
    print(f"Connected to {ssid}")

  def get_connected_network(self):
    """Retrieve the currently connected network."""
    active_connections = self.network_manager_interface.ActiveConnections()
    for conn_path in active_connections:
      conn = self.bus.get_object('org.freedesktop.NetworkManager', conn_path)
      conn_interface = dbus.Interface(conn, 'org.freedesktop.NetworkManager.Connection.Active')
      self.connected_network = conn_interface.Get('org.freedesktop.NetworkManager.Connection.Active', 'Id')
    return self.connected_network


class WifiManagerUI:
  def __init__(self, wifi_manager):
    self.wifi_manager = wifi_manager
    self.selected_network = None
    self.password = ""

  def draw_network_list(self):
    networks = self.wifi_manager.networks
    rl.draw_text("Available Networks", 50, 50, 20, rl.DARKGRAY)

    for i, network in enumerate(networks):
      y_offset = 100 + i * 30
      if rl.gui_button(rl.Rectangle(50, y_offset, 200, 25), network):
        self.selected_network = network
        print(f"Selected network: {self.selected_network}")


def main():
  rl.init_window(800, 600, "WiFi Manager")
  rl.set_target_fps(60)

  wifi_manager = WifiManager()
  wifi_manager.get_available_networks()

  wifi_ui = WifiManagerUI(wifi_manager)

  while not rl.window_should_close():
    rl.begin_drawing()
    rl.clear_background(rl.RAYWHITE)

    wifi_ui.draw_network_list()

    rl.end_drawing()

  rl.close_window()


if __name__ == "__main__":
  main()
