#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cassert>
#include <cstdio>
#include <thread>

#include "libyuv.h"

#include "cereal/visionipc/visionipc_server.h"
#include "selfdrive/common/clutil.h"
#include "selfdrive/common/params.h"
#include "selfdrive/common/swaglog.h"
#include "selfdrive/common/util.h"
#include "selfdrive/hardware/hw.h"

#ifdef QCOM
#include "selfdrive/camerad/cameras/camera_qcom.h"
#elif QCOM2
#include "selfdrive/camerad/cameras/camera_qcom2.h"
#elif WEBCAM
#include "selfdrive/camerad/cameras/camera_webcam.h"
#else
#include "selfdrive/camerad/cameras/camera_frame_stream.h"
#endif

ExitHandler do_exit;

void party() {
  MultiCameraState cameras = {};
  VisionIpcServer vipc_server("camerad", CLContext::deviceId(), CLContext::context());

  cameras_init(&vipc_server, &cameras);
  cameras_open(&cameras);

  vipc_server.start_listener();

  cameras_run(&cameras);
}

#ifdef QCOM
#include "CL/cl_ext_qcom.h"
#endif

int main(int argc, char *argv[]) {
  set_realtime_priority(53);
  if (Hardware::EON()) {
    set_core_affinity(2);
  } else if (Hardware::TICI()) {
    set_core_affinity(6);
  }

   // TODO: do this for QCOM2 too
#if defined(QCOM)
  const cl_context_properties props[] = {CL_CONTEXT_PRIORITY_HINT_QCOM, CL_PRIORITY_HINT_HIGH_QCOM, 0};
  CLContext::init(CL_DEVICE_TYPE_DEFAULT, props);
#else
  CLContext::init(CL_DEVICE_TYPE_DEFAULT);
#endif

  party();
}
