#pragma once

#include <cassert>
#include <optional>
#include <map>
#include <QtDBus>
#include <QTimer>
#include <iostream>
#include <dbus/dbus.h>
#include <variant>

#include "selfdrive/ui/qt/network/networkmanager.h"

enum class SecurityType {
  OPEN,
  WPA,
  UNSUPPORTED
};
enum class ConnectedType {
  DISCONNECTED,
  CONNECTING,
  CONNECTED
};
enum class NetworkType {
  NONE,
  WIFI,
  CELL,
  ETHERNET
};

typedef QMap<QString, QVariantMap> Connection;
typedef QVector<QVariantMap> IpConfig;

using DBusVariant = std::variant<int32_t, uint32_t, double, std::string, bool,
                                 std::map<std::string, std::variant<int32_t, uint32_t, double, std::string, bool>>>;

struct Network {
  QString ssid;
  unsigned int strength;
  ConnectedType connected;
  SecurityType security_type;
};
bool compare_by_strength(const Network &a, const Network &b);
inline int strengthLevel(unsigned int strength) { return std::clamp((int)round(strength / 33.), 0, 3); }

class WifiManager : public QObject {
  Q_OBJECT

public:
  QMap<QString, Network> seenNetworks;
  QMap<QDBusObjectPath, QString> knownConnections;
  QString ipv4_address;
  bool tethering_on = false;
  bool ipv4_forward = false;

  explicit WifiManager(QObject* parent);
  ~WifiManager();
  void start();
  void stop();
  void requestScan();
  void forgetConnection(const QString &ssid);
  bool isKnownConnection(const QString &ssid);
  std::optional<QDBusPendingCall> activateWifiConnection(const QString &ssid);
  NetworkType currentNetworkType();
  void updateGsmSettings(bool roaming, QString apn, bool metered);
  void connect(const Network &ssid, const bool is_hidden = false, const QString &password = {}, const QString &username = {});

  // Tethering functions
  void setTetheringEnabled(bool enabled);
  bool isTetheringEnabled();
  void changeTetheringPassword(const QString &newPassword);
  QString getTetheringPassword();

private:

  DBusConnection *dbus;
  DBusError error;

  template <typename T>
  T extractFromMessage(DBusMessage *message) {
    DBusMessageIter args;
    dbus_bool_t ret = dbus_message_iter_init(message, &args);
    assert(ret);

    if constexpr (std::is_same_v<T, uint32_t>) {
      assert(DBUS_TYPE_VARIANT == dbus_message_iter_get_arg_type(&args) && "Argument is not an variant");
      DBusMessageIter variantIter;
      dbus_message_iter_recurse(&args, &variantIter);
      assert(DBUS_TYPE_UINT32 == dbus_message_iter_get_arg_type(&variantIter) && "Argument is not an uint32.");
      uint32_t value;
      dbus_message_iter_get_basic(&variantIter, &value);
      return value;

    } else if constexpr (std::is_same_v<T, std::string>) {
      assert(DBUS_TYPE_STRING == dbus_message_iter_get_arg_type(&args) && "Argument is not a string.");
      const char *str;
      dbus_message_iter_get_basic(&args, &str);
      return std::string(str);

    } else if constexpr (std::is_same_v<T, std::vector<std::string>>) {
      // assert(DBUS_TYPE_ARRAY == dbus_message_iter_get_arg_type(&args) && "Argument is not an array.");
      std::vector<std::string> result;
      auto type = dbus_message_iter_get_arg_type(&args);
      if (type == DBUS_TYPE_ARRAY) {
        DBusMessageIter subIter;
        dbus_message_iter_recurse(&args, &subIter);
        // Iterate over the array of strings
        while (dbus_message_iter_get_arg_type(&subIter) != DBUS_TYPE_INVALID) {
          const char *str;
          dbus_message_iter_get_basic(&subIter, &str);
          result.emplace_back(str);
          dbus_message_iter_next(&subIter);
        }
      } else if (type == DBUS_TYPE_VARIANT) {
        DBusMessageIter variant;
        dbus_message_iter_recurse(&args, &variant);
        if (dbus_message_iter_get_arg_type(&variant) == DBUS_TYPE_ARRAY) {
          DBusMessageIter array_iter;
          dbus_message_iter_recurse(&variant, &array_iter);

          // Iterate over each object path in the array
          while (dbus_message_iter_get_arg_type(&array_iter) == DBUS_TYPE_OBJECT_PATH) {
            const char *activeConnection;
            dbus_message_iter_get_basic(&array_iter, &activeConnection);

            result.push_back(std::string(activeConnection));
            dbus_message_iter_next(&array_iter);
          }
        }
      }

      return result;

    } else if constexpr (std::is_same_v<T, std::map<std::string, DBusVariant>>) {
      T result;
      DBusMessageIter dict;
      dbus_message_iter_recurse(&args, &dict);
      while (dbus_message_iter_get_arg_type(&dict) != DBUS_TYPE_INVALID) {
        DBusMessageIter entry;
        dbus_message_iter_recurse(&dict, &entry);

        // The dictionary entries are key-value pairs, the key is a string, the value can be various types
        const char *key;
        dbus_message_iter_get_basic(&entry, &key);  // Get the key (property name)

        std::cout << "Property: " << key << std::endl;
        // Move to the value (skip key)
        dbus_message_iter_next(&entry);

        // Now handle the value (variant type)
        if (dbus_message_iter_get_arg_type(&entry) == DBUS_TYPE_VARIANT) {
          DBusMessageIter variant;
          dbus_message_iter_recurse(&entry, &variant);

          // Store the value in a variant (handle multiple types)
          switch (dbus_message_iter_get_arg_type(&variant)) {
            case DBUS_TYPE_INT32: {
              int32_t value;
              dbus_message_iter_get_basic(&variant, &value);
              result[key] = value;
              std::cout << "Value (int32): " << value << std::endl;
              break;
            }
            case DBUS_TYPE_BYTE: {
              uint8_t value;
              dbus_message_iter_get_basic(&variant, &value);
              result[key] = static_cast<uint32_t>(value);
              std::cout << "Value (uint8): " << static_cast<uint32_t>(value) << std::endl;
              break;
            }
            case DBUS_TYPE_UINT32: {
              uint32_t value;
              dbus_message_iter_get_basic(&variant, &value);
              result[key] = value;
              std::cout << "Value (uint32): " << value << std::endl;
              break;
            }
            case DBUS_TYPE_UINT16: {
              uint16_t value;
              dbus_message_iter_get_basic(&variant, &value);
              result[key] = static_cast<uint32_t>(value);
              break;
            }
            case DBUS_TYPE_DOUBLE: {
              double value;
              dbus_message_iter_get_basic(&variant, &value);
              result[key] = value;
              std::cout << "Value (double): " << value << std::endl;
              break;
            }
            case DBUS_TYPE_STRING: {
              const char *value;
              dbus_message_iter_get_basic(&variant, &value);
              result[key] = std::string(value);
              std::cout << "Value (string): " << value << std::endl;
              break;
            }
            case DBUS_TYPE_BOOLEAN: {
              dbus_bool_t value;
              dbus_message_iter_get_basic(&variant, &value);
              result[key] = static_cast<bool>(value);
              std::cout << "Value (boolean): " << value << std::endl;
              break;
            }
            case DBUS_TYPE_ARRAY: {
              DBusMessageIter array_iter;
              dbus_message_iter_recurse(&variant, &array_iter);

              std::string byte_array;
              while (dbus_message_iter_get_arg_type(&array_iter) == DBUS_TYPE_BYTE) {
                uint8_t byte_value;
                dbus_message_iter_get_basic(&array_iter, &byte_value);
                byte_array.push_back(static_cast<char>(byte_value));
                dbus_message_iter_next(&array_iter);
              }
              result[key] = byte_array;
              std::cout << "Value (byte array as string): " << byte_array << std::endl;
              break;
            }
            default:
              std::cout << key << "***************Unsupported type: " << dbus_message_iter_get_arg_type(&variant) << std::endl;
              break;
          }
        }

        dbus_message_iter_next(&dict);
      }
      return result;

    } else {
      // If T is unsupported, trigger a static assert
      assert(0 && "Unsupported type for extraction.");
    }
  }

  template <typename T=void, typename... Args>
  T sendMethodCall(const char *path, const char *interface, const char *method, Args &&...args) {
    DBusMessage *msg = dbus_message_new_method_call(NM_DBUS_SERVICE, path, interface, method);
    assert(msg != nullptr && "Failed to create D-Bus message");

    // Function to append the arguments one by one
    auto appendArgs = [&msg](auto &&arg) {
      using ArgType = std::decay_t<decltype(arg)>;
      if constexpr (std::is_same_v<ArgType, int>) {
        int v = arg;
        return dbus_message_append_args(msg, DBUS_TYPE_INT32, &v, DBUS_TYPE_INVALID);
      } else if constexpr (std::is_same_v<ArgType, const char *>) {
        const char *v = arg;
        return dbus_message_append_args(msg, DBUS_TYPE_STRING, &v, DBUS_TYPE_INVALID);
      } else {
        return false;  // Unsupported type
      }
    };

    bool success = (appendArgs(args) && ...);
    assert(success);

    DBusMessage *reply = dbus_connection_send_with_reply_and_block(dbus, msg, DBUS_TIMEOUT_INFINITE, &error);
    dbus_message_unref(msg);

    if (dbus_error_is_set(&error)) {
      std::cerr << "Error in D-Bus method call: " << error.message << std::endl;
      dbus_error_free(&error);
      assert(0);
    }
    if constexpr (std::is_void_v<T>) {
      dbus_message_unref(reply);
      return T();
    } else {
      T result = extractFromMessage<T>(reply);
      dbus_message_unref(reply);
      return result;
    }
  }


  QString adapter;  // Path to network manager wifi-device
  QTimer timer;
  unsigned int raw_adapter_state = NM_DEVICE_STATE_UNKNOWN;  // Connection status https://developer.gnome.org/NetworkManager/1.26/nm-dbus-types.html#NMDeviceState
  QString connecting_to_network;
  QString tethering_ssid;
  const QString defaultTetheringPassword = "swagswagcomma";
  QString activeAp;
  QDBusObjectPath lteConnectionPath;

  QString getAdapter(const uint = NM_DEVICE_TYPE_WIFI);
  uint getAdapterType(const std::string &path);
  QString getIp4Address();
  void deactivateConnectionBySsid(const QString &ssid);
  void deactivateConnection(const std::string &path);
  std::vector<std::string> getActiveConnections();
  QByteArray get_property(const QString &network_path, const QString &property);
  SecurityType getSecurityType(const std::map<std::string, DBusVariant> &properties);
  QDBusObjectPath getConnectionPath(const QString &ssid);
  Connection getConnectionSettings(const QDBusObjectPath &path);
  void initConnections();
  void setup();
  void refreshNetworks();
  void activateModemConnection(const QDBusObjectPath &path);
  void addTetheringConnection();
  void setCurrentConnecting(const QString &ssid);

signals:
  void wrongPassword(const QString &ssid);
  void refreshSignal();

private slots:
  void stateChange(unsigned int new_state, unsigned int previous_state, unsigned int change_reason);
  void propertyChange(const QString &interface, const QVariantMap &props, const QStringList &invalidated_props);
  void deviceAdded(const QDBusObjectPath &path);
  void connectionRemoved(const QDBusObjectPath &path);
  void newConnection(const QDBusObjectPath &path);
  void refreshFinished(QDBusPendingCallWatcher *call);
  void tetheringActivated(QDBusPendingCallWatcher *call);
};
