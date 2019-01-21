#include "base/placement_new.h"

#ifndef TEST_BUILD

void* operator new(size_t sz, void* here) {
  return here;
}

void* operator new[](size_t sz, void* here) {
  return here;
}

#endif
