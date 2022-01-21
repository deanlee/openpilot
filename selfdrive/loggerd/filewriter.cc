#include "selfdrive/loggerd/filewriter.h"

#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>

#include <cassert>

FileWriter::FileWriter(size_t prealloc_size_mb) : prealloc_size(prealloc_size_mb * 1024 * 1024) {}

FileWriter::~FileWriter() {
  if (fd != -1) {
    close();
  }
}

bool FileWriter::open(const char *fn) {
  fd = ::open(fn, O_WRONLY | O_CREAT | O_TRUNC, 0664);
  assert(fd >= 0);
  int ret = fallocate(fd, 0, 0, prealloc_size);
  assert(ret == 0);
  lseek(fd, 0, SEEK_SET);
  return true;
}

void FileWriter::close() {
  if (written < prealloc_size) {
    ftruncate(fd, written);
  }
  ::close(fd);
  fd = -1;
}

int FileWriter::write(void *data, size_t size) {
  int ret = ::write(fd, data, size);
  if (ret >= 0) {
    written += size;
  }
  return ret;
}
