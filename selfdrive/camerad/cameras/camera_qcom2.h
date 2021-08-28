#pragma once

#include <pthread.h>

#include <cstdint>

#include <media/cam_req_mgr.h>

#include "selfdrive/camerad/cameras/camera_common.h"
#include "selfdrive/common/util.h"

#define FRAME_BUF_COUNT 4
#define DEBAYER_LOCAL_WORKSIZE 16

struct CmdBuffer {
  uint32_t handle;
  void *address;
  uint64_t size;
};

class ReqManager {
public:
  ReqManager();
  ~ReqManager();
  bool init();
  int32_t createSession();
  void destroySession(int32_t session_handle);
  int32_t linkDevice(int32_t session_handle, int32_t dev_hdl_1, int32_t dev_hdl_2);
  void unlinkDevice(int32_t session_handle, int32_t dev_handle);
  CmdBuffer allocCmdBuffer(size_t len, uint64_t align = 8, uint32_t flags = CAM_MEM_FLAG_KMD_ACCESS | CAM_MEM_FLAG_UMD_ACCESS | CAM_MEM_FLAG_CMD_BUF_TYPE | CAM_MEM_FLAG_HW_READ_WRITE);
  void freeCmdBuffer(CmdBuffer &buf);
  unique_fd video0_fd;
};

typedef struct CameraState {
  MultiCameraState *multi_cam_state;
  CameraInfo ci;

  std::mutex exp_lock;

  int exposure_time;
  bool dc_gain_enabled;
  float analog_gain_frac;

  float cur_ev[3];
  float min_ev, max_ev;

  float measured_grey_fraction;
  float target_grey_fraction;
  int gain_idx;

  unique_fd sensor_fd;
  unique_fd csiphy_fd;

  int camera_num;

  uint32_t session_handle;

  uint32_t sensor_dev_handle;
  uint32_t isp_dev_handle;
  uint32_t csiphy_dev_handle;

  int32_t link_handle;

  int buf0_handle;
  int buf_handle[FRAME_BUF_COUNT];
  int sync_objs[FRAME_BUF_COUNT];
  int request_ids[FRAME_BUF_COUNT];
  int request_id_last;
  int frame_id_last;
  int idx_offset;
  bool skipped;

  CameraBuf buf;
} CameraState;

typedef struct MultiCameraState {
  int device;
  ReqManager req_mgr;
  // unique_fd video0_fd;
  unique_fd video1_fd;
  unique_fd isp_fd;
  int device_iommu;
  int cdm_iommu;


  CameraState road_cam;
  CameraState wide_road_cam;
  CameraState driver_cam;

  pthread_mutex_t isp_lock;

  SubMaster *sm;
  PubMaster *pm;
} MultiCameraState;
