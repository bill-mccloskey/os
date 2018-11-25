#ifndef string_h
#define string_h

#include "types.h"

extern "C" {

void* memcpy(void* dest, const void* src, size_t n);
int memcmp(const void* s1, const void* s2, size_t n);

};

#endif
