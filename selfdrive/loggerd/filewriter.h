#pragma once

#include <stddef.h>

class FileWriter {
public:
  ~FileWriter();
  FileWriter(size_t prealloc_size_mb);
  bool open(const char *file);
  void close();
  int write(void *data, size_t size);

private:
  size_t prealloc_size = 0;
  size_t written = 0;
  int fd = -1;
};

