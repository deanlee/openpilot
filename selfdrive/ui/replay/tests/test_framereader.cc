
#include <thread>

#define CATCH_CONFIG_MAIN
#include "catch2/catch.hpp"

#include "selfdrive/ui/replay/framereader.h"


TEST_CASE("framereader") {
  {
    FrameReader reader("/home/deanlee/fcamera.hevc");
    reader.get(1000);
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
//   REQUIRE(ret1 == ret2);
  }
  
}