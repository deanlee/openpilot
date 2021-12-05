#include <cassert>

#include "selfdrive/common/params.h"
#include "selfdrive/common/util.h"
#include "selfdrive/hardware/hw.h"


#ifdef QCOM
#include "selfdrive/camerad/cameras/camera_qcom.h"
#elif QCOM2
#include "selfdrive/camerad/cameras/camera_qcom2.h"
#elif WEBCAM
#include "selfdrive/camerad/cameras/camera_webcam.h"
#else
#include "selfdrive/camerad/cameras/camera_replay.h"
#endif

ExitHandler do_exit;

int main(int argc, char *argv[]) {
  if (!Hardware::PC()) {
    int ret;
    ret = set_realtime_priority(53);
    assert(ret == 0);
    ret = set_core_affinity({Hardware::EON() ? 2 : 6});
    assert(ret == 0 || Params().getBool("IsOffroad")); // failure ok while offroad due to offlining cores
  }

  MultiCameraState s;
  s.start();
}
