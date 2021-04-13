#if defined(QCOM) && !defined(QCOM_REPLAY)
#include "cameras/camera_qcom.h"
#elif QCOM2
#include "cameras/camera_qcom2.h"
#elif WEBCAM
#include "cameras/camera_webcam.h"
#else
#include "cameras/camera_frame_stream.h"
#endif
#include "common/util.h"

int main(int argc, char *argv[]) {
  set_realtime_priority(53);
  if (Hardware::EON()) {
    set_core_affinity(2);
  } else if (Hardware::TICI()) {
    set_core_affinity(6);
  }

  CameraServer s;
  s.start();
}
