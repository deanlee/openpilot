#define CATCH_CONFIG_MAIN
#define CATCH_CONFIG_ENABLE_BENCHMARKING
#include "catch2/catch.hpp"
#include "selfdrive/common/util.h"
#include "selfdrive/loggerd/filewriter.h"

TEST_CASE("FileWriter") {
  char filename[] = "/tmp/test_write_XXXXXX";
  int fd = mkstemp(filename);

  FileWriter file(50);
  file.open(filename);
  std::string data;
  for (int i = 0; i < 100; ++i) {
    std::string chunk = "aaaaaaa";
    file.write((void *)chunk.c_str(), chunk.length());
    data += chunk;
  }
  file.close();

  std::string content = util::read_file(filename);
  REQUIRE(data == content);
  close(fd);
}

TEST_CASE("benchmark") {
  char filename[] = "/tmp/test_write_XXXXXX";
  int fd = mkstemp(filename);
  BENCHMARK("FileWriter") {
    FileWriter file(5);
    file.open(filename);
    std::string chunk = "aaaaaaa";
    for (int i = 0; i < 10000; ++i) {
      file.write((void *)chunk.c_str(), chunk.length());
    }
    file.close();
  };

  BENCHMARK("fwrite") {
    FILE *fp = fopen(filename, "wb");
    std::string chunk = "aaaaaaa";
    for (int i = 0; i < 10000; ++i) {
      fwrite(chunk.c_str(), chunk.length(), 1, fp);
    }
    fclose(fp);
  };
  close(fd);
}
