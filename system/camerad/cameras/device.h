#pragma

#include <cstdint>
#include <optional>
class Device {
public:
  Device();
  ~Device();
  bool acquireDevice(int fd, int32_t session_handle, void *data, uint32_t num_resources = 1);
  int configDevice(uint64_t packet_handle);
  int releaseDevice();

  int handle;

private:
  int fd_;
  int32_t session_handle_;
};

int do_cam_control(int fd, int op_code, void *handle, int size);


class DeviceLink {
public:
  DeviceLink();
  bool link(int32_t dev_handle1, int32_t dev_handle2);
  int32_t link_handle = -1;
};