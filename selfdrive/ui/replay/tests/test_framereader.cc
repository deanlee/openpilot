#define CATCH_CONFIG_MAIN
#include "catch2/catch.hpp"
#include "selfdrive/ui/replay/framereader.h"
#include "selfdrive/common/util.h"

void process_thread(FrameReader *r) {
  r->process();
}

TEST_CASE("FrameReader") {
  SECTION("abort input") {
    {
      FrameReader *reader = new FrameReader("https://commadataci.blob.core.windows.net/openpilotci/0c94aa1e1296d7c6/2021-05-05--19-48-37/0/qcamera.ts");
      std::thread t = std::thread(process_thread, reader);
      util::sleep_for(50);
      delete reader;
      t.join();
    }
  }
}