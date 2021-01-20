#include <assert.h>
#include <string>
#include "common/swaglog.h"
#include "logger.h"

int main(int argc, char** argv) {
  Logger logger(LOG_ROOT, "bootlog", false);
  std::string segment_path = logger.next(nullptr);
  LOGW("bootlog to %s", segment_path.c_str());

  localtime_r(&rawtime, &timeinfo);
  strftime(filename, sizeof(filename),
           "%Y-%m-%d--%H-%M-%S.bz2", &timeinfo);

  std::string path = LOG_ROOT + "/boot/" + std::string(filename);
  LOGW("bootlog to %s", path.c_str());

  // Open bootlog
  int r = logger_mkpath((char*)path.c_str());
  assert(r == 0);

  BZFile bz_file(path.c_str());

  // Write initdata
  bz_file.write(logger_build_init_data().asBytes());

  auto bytes = msg.toBytes();
  logger.write(bytes.begin(), bytes.size(), false);
  return 0;
}
