#ifndef types_h
#define types_h

#include <stdint.h>
#include <string.h>

using phys_addr_t = uintptr_t;
using virt_addr_t = uintptr_t;

static const int kPageSize = 4096;

#endif  // types_h

