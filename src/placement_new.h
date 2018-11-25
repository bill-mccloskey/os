#ifndef placement_new_h
#define placement_new_h

#include "types.h"

inline void* operator new(size_t sz, void* here) {
  return here;
}

inline void* operator new[](size_t sz, void* here) {
  return here;
}

#endif
