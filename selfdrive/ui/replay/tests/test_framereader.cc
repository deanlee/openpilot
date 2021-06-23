#define CATCH_CONFIG_MAIN
#include "catch2/catch.hpp"
#include "selfdrive/ui/replay/framereader.h"
#include "selfdrive/common/util.h"
#include <random>

void process_thread(FrameReader *r) {
  r->process();
}

TEST_CASE("FrameReader") {
  SECTION("process&get") {
    FrameReader fr("https://commadataci.blob.core.windows.net/openpilotci/0c94aa1e1296d7c6/2021-05-05--19-48-37/0/fcamera.hevc");
    bool ret = fr.process();
    REQUIRE(ret == true);
    REQUIRE(fr.valid() == true);
    REQUIRE(fr.getFrameCount() == 1200);

    // random get 50 frames
    srand(time(NULL));
    for (int i = 0; i < 20; ++i) {
      int idx = rand() % (fr.getFrameCount() - 1);
      REQUIRE(fr.get(idx) != nullptr);
    }
    // sequence get 50 frames {
    for (int i = 0; i < 50; ++i) {
      REQUIRE(fr.get(i) != nullptr);
    }
  }
  SECTION("process with timeout") {
    FrameReader fr("https://commadataci.blob.core.windows.net/openpilotci/0c94aa1e1296d7c6/2021-05-05--19-48-37/0/fcamera.hevc", 3);
    bool ret = fr.process();
    REQUIRE(ret == false);
    REQUIRE(fr.valid() == false);
    REQUIRE(fr.getFrameCount() < 1200);
  }
}
