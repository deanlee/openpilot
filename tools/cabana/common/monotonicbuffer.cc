#include "tools/cabana/common/monotonicbuffer.h"

#include <algorithm>
#include <cassert>
#include <memory>

void *MonotonicBuffer::allocate(size_t bytes, size_t alignment) {
  assert(bytes > 0);
  void *p = std::align(alignment, bytes, current_buf, available);
  if (p == nullptr) {
    available = next_buffer_size = std::max(next_buffer_size, bytes);
    current_buf = buffers.emplace_back(std::aligned_alloc(alignment, next_buffer_size));
    next_buffer_size *= growth_factor;
    p = current_buf;
  }

  current_buf = (char *)current_buf + bytes;
  available -= bytes;
  return p;
}

MonotonicBuffer::~MonotonicBuffer() {
  for (auto buf : buffers) {
    free(buf);
  }
}
