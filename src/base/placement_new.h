#ifndef placement_new_h
#define placement_new_h

#include <string.h>

void* operator new(size_t sz, void* here) noexcept;
void* operator new[](size_t sz, void* here) noexcept;

#endif
