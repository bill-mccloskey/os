#ifndef types_h
#define types_h

#include <stdint.h>
#include <string.h>

using phys_addr_t = uintptr_t;
using virt_addr_t = uintptr_t;

static const int kPageSize = 4096;

static const uint64_t kLargePageSize = 1 << 21;
static const uint64_t kHugePageSize = 1 << 30;

#ifdef TESTING
#define FINAL_IF_RELEASE /* empty */
#else
#define FINAL_IF_RELEASE final
#endif

#endif  // types_h

