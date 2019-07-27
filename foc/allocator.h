#pragma once

namespace foc {

class MallocAllocator {
 public:
  void *allocate(size_t size, size_t) { return malloc(size); }
  void deallocate(void *ptr, size_t) { free(ptr); }
};

}  // namespace foc
