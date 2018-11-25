#include "string.h"

void* memcpy(void* dest, const void* src, size_t n) {
  for (size_t i = 0; i < n; i++) {
    ((char*)dest)[i] = ((const char*)src)[i];
  }
  return dest;
}

int memcmp(const void* s1, const void* s2, size_t n) {
  for (size_t i = 0; i < n; i++) {
    signed char c1 = ((signed char*)s1)[i];
    signed char c2 = ((signed char*)s2)[i];
    if (c1 != c2) return c1 - c2;
  }
  return 0;
}
