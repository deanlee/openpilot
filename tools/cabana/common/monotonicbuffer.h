#pragma once

#include <cstddef>
#include <deque>

class MonotonicBuffer {
public:
  MonotonicBuffer(size_t initial_size) : next_buffer_size(initial_size) {}
  ~MonotonicBuffer();
  void *allocate(size_t bytes, size_t alignment = 16ul);
  void deallocate(void *p) {}

private:
  void *current_buf = nullptr;
  size_t next_buffer_size = 0;
  size_t available = 0;
  std::deque<void *> buffers;
  static constexpr float growth_factor = 1.5;
};
