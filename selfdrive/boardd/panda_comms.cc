#include "selfdrive/boardd/panda.h"

#include <cassert>
#include <optional>
#include <stdexcept>
#include <memory>
#include <variant>
#include "common/swaglog.h"

static libusb_context *init_usb_ctx() {
  libusb_context *context = nullptr;
  int err = libusb_init(&context);
  if (err != 0) {
    LOGE("libusb initialization error");
    return nullptr;
  }

#if LIBUSB_API_VERSION >= 0x01000106
  libusb_set_option(context, LIBUSB_OPTION_LOG_LEVEL, LIBUSB_LOG_LEVEL_INFO);
#else
  libusb_set_debug(context, 3);
#endif
  return context;
}

// Open a USB device with a specific serial number or return a list of all serial numbers
std::variant<std::vector<std::string>, libusb_device_handle *>
open_or_list_device(libusb_context *ctx, std::optional<std::string> serial = std::nullopt) {
  std::vector<std::string> serials;
  libusb_device_handle *dev_handle = nullptr;
  libusb_device **dev_list = nullptr;

  size_t num_devices = libusb_get_device_list(ctx, &dev_list);
  for (size_t i = 0; i < num_devices; ++i) {
    libusb_device_descriptor desc;
    libusb_device_handle *handle = nullptr;
    if (libusb_get_device_descriptor(dev_list[i], &desc) == 0 &&
        desc.idVendor == 0xbbaa && desc.idProduct == 0xddcc &&
        libusb_open(dev_list[i], &handle) == 0) {
      unsigned char s[256] = {'\0'};
      int ret = libusb_get_string_descriptor_ascii(handle, desc.iSerialNumber, s, std::size(s));
      if (ret >= 0) {
        auto hw_serial = std::string((char *)s, ret);
        serials.emplace_back(hw_serial);
        if (serial && (serial->empty() || *serial == hw_serial)) {
          dev_handle = handle;
          break;
        }
        libusb_close(handle);
      }
    }
  }
  libusb_free_device_list(dev_list, 1);
  if (serial) return dev_handle;
  return serials;
}

PandaUsbHandle::PandaUsbHandle(std::string serial) : PandaCommsHandle(serial) {
  ctx = init_usb_ctx();
  if (ctx) {
    dev_handle = std::get<libusb_device_handle *>(open_or_list_device(ctx, serial));
    if (dev_handle) {
      if (libusb_kernel_driver_active(dev_handle, 0) == 1) {
        libusb_detach_kernel_driver(dev_handle, 0);
      }

      int err = libusb_set_configuration(dev_handle, 1);
      if (err == 0) {
        libusb_claim_interface(dev_handle, 0);
        return;
      }
    }
  }
  cleanup();
  throw std::runtime_error("Error connecting to panda");
}

PandaUsbHandle::~PandaUsbHandle() {
  std::lock_guard lk(hw_lock);
  cleanup();
  connected = false;
}

void PandaUsbHandle::cleanup() {
  if (dev_handle) {
    libusb_release_interface(dev_handle, 0);
    libusb_close(dev_handle);
  }

  if (ctx) {
    libusb_exit(ctx);
  }
}

std::vector<std::string> PandaUsbHandle::list() {
  static std::unique_ptr<libusb_context, decltype(&libusb_exit)> context(init_usb_ctx(), libusb_exit);
  return std::get<std::vector<std::string>>(open_or_list_device(context.get(), std::nullopt));
}

void PandaUsbHandle::handle_usb_issue(int err, const char func[]) {
  LOGE_100("usb error %d \"%s\" in %s", err, libusb_strerror((enum libusb_error)err), func);
  if (err == LIBUSB_ERROR_NO_DEVICE) {
    LOGE("lost connection");
    connected = false;
  }
  // TODO: check other errors, is simply retrying okay?
}

int PandaUsbHandle::control_write(uint8_t bRequest, uint16_t wValue, uint16_t wIndex, unsigned int timeout) {
  int err;
  const uint8_t bmRequestType = LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE;

  if (!connected) {
    return LIBUSB_ERROR_NO_DEVICE;
  }

  std::lock_guard lk(hw_lock);
  do {
    err = libusb_control_transfer(dev_handle, bmRequestType, bRequest, wValue, wIndex, NULL, 0, timeout);
    if (err < 0) handle_usb_issue(err, __func__);
  } while (err < 0 && connected);

  return err;
}

int PandaUsbHandle::control_read(uint8_t bRequest, uint16_t wValue, uint16_t wIndex, unsigned char *data, uint16_t wLength, unsigned int timeout) {
  int err;
  const uint8_t bmRequestType = LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE;

  if (!connected) {
    return LIBUSB_ERROR_NO_DEVICE;
  }

  std::lock_guard lk(hw_lock);
  do {
    err = libusb_control_transfer(dev_handle, bmRequestType, bRequest, wValue, wIndex, data, wLength, timeout);
    if (err < 0) handle_usb_issue(err, __func__);
  } while (err < 0 && connected);

  return err;
}

int PandaUsbHandle::bulk_write(unsigned char endpoint, unsigned char* data, int length, unsigned int timeout) {
  int err;
  int transferred = 0;

  if (!connected) {
    return 0;
  }

  std::lock_guard lk(hw_lock);
  do {
    // Try sending can messages. If the receive buffer on the panda is full it will NAK
    // and libusb will try again. After 5ms, it will time out. We will drop the messages.
    err = libusb_bulk_transfer(dev_handle, endpoint, data, length, &transferred, timeout);

    if (err == LIBUSB_ERROR_TIMEOUT) {
      LOGW("Transmit buffer full");
      break;
    } else if (err != 0 || length != transferred) {
      handle_usb_issue(err, __func__);
    }
  } while (err != 0 && connected);

  return transferred;
}

int PandaUsbHandle::bulk_read(unsigned char endpoint, unsigned char* data, int length, unsigned int timeout) {
  int err;
  int transferred = 0;

  if (!connected) {
    return 0;
  }

  std::lock_guard lk(hw_lock);

  do {
    err = libusb_bulk_transfer(dev_handle, endpoint, data, length, &transferred, timeout);

    if (err == LIBUSB_ERROR_TIMEOUT) {
      break; // timeout is okay to exit, recv still happened
    } else if (err == LIBUSB_ERROR_OVERFLOW) {
      comms_healthy = false;
      LOGE_100("overflow got 0x%x", transferred);
    } else if (err != 0) {
      handle_usb_issue(err, __func__);
    }

  } while (err != 0 && connected);

  return transferred;
}
