#include <cassert>

#include "common/params.h"
#include "common/util.h"
#include "system/hardware/hw.h"
#include "system/camerad/cameras/camera_qcom2.h"

int main(int argc, char *argv[]) {
  if (Hardware::PC()) {
    printf("exiting, camerad is not meant to run on PC\n");
    return 0;
  }

  int ret = util::set_realtime_priority(53);
  assert(ret == 0);
  ret = util::set_core_affinity({6});
  assert(ret == 0 || Params().getBool("IsOffroad")); // failure ok while offroad due to offlining cores

  MultiCameraState camera_server;
  camera_server.run();
  return 0;
}
