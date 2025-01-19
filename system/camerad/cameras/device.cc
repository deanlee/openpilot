#include "openpilot/system/camerad/cameras/device.h"
#include <unistd.h>
#include "third_party/linux/include/media/cam_defs.h"

// extern do_cam_control
Device::Device() {}

Device::~Device() {
  // Close the file descriptor if it's valid
  // if (fd >= 0) {
    // close(fd);
  // }
}

bool Device::acquireDevice(int fd, int32_t session_handle, void *data, uint32_t num_resources) {
  struct cam_acquire_dev_cmd cmd = {
    .session_handle = session_handle,
    .handle_type = CAM_HANDLE_USER_POINTER,
    .num_resources = data ? num_resources : 0,
    .resource_hdl = (uint64_t)data,
  };
  int err = do_cam_control(fd_, CAM_ACQUIRE_DEV, &cmd, sizeof(cmd));
  if (err != 0) {
    return false;
  }

  fd_ = fd;
  session_handle_ = session_handle;
  handle = cmd.dev_handle;
  return true;
}


int Device::configDevice(uint64_t packet_handle) {
  struct cam_config_dev_cmd cmd = {
    .session_handle = session_handle_,
    .dev_handle = handle,
    .packet_handle = packet_handle,
  };
  return do_cam_control(fd_, CAM_CONFIG_DEV, &cmd, sizeof(cmd));
}

int Device::releaseDevice() {
  struct cam_release_dev_cmd cmd = {
    .session_handle = session_handle_,
    .dev_handle = handle,
  };
  return do_cam_control(fd_, CAM_RELEASE_DEV, &cmd, sizeof(cmd));
}


bool DeviceLink::link(int32_t dev_handle1, int32_t dev_handle2) {
struct cam_req_mgr_link_info req_mgr_link_info = {0};
  req_mgr_link_info.session_hdl = session_handle;
  req_mgr_link_info.num_devices = 2;
  req_mgr_link_info.dev_hdls[0] = dev_handle1;
  req_mgr_link_info.dev_hdls[1] = sensor_dev.handle;
  int ret = do_cam_control(m->video0_fd, CAM_REQ_MGR_LINK, &req_mgr_link_info, sizeof(req_mgr_link_info));
  link_handle = req_mgr_link_info.link_hdl;
  LOGD("link: %d session: 0x%X isp: 0x%X sensors: 0x%X link: 0x%X", ret, session_handle, dev_handle1, sensor_dev.handle, link_handle);

  struct cam_req_mgr_link_control req_mgr_link_control = {0};
  req_mgr_link_control.ops = CAM_REQ_MGR_LINK_ACTIVATE;
  req_mgr_link_control.session_hdl = session_handle;
  req_mgr_link_control.num_links = 1;
  req_mgr_link_control.link_hdls[0] = link_handle;
  ret = do_cam_control(m->video0_fd, CAM_REQ_MGR_LINK_CONTROL, &req_mgr_link_control, sizeof(req_mgr_link_control));
  LOGD("link control: %d", ret);

  ret = device_control(csiphy_fd, CAM_START_DEV, session_handle, csiphy_dev.handle);
  LOGD("start csiphy: %d", ret);
  ret = device_control(m->isp_fd, CAM_START_DEV, session_handle, dev_handle1);
  LOGD("start isp: %d", ret);
}