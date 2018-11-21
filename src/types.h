#ifndef types_h
#define types_h

using uint8_t = unsigned char;
using uint16_t = unsigned short;
using uint32_t = unsigned int;
using uint64_t = unsigned long;

using uintptr_t = unsigned long;
using size_t = unsigned long;

using phys_addr_t = uintptr_t;
using virt_addr_t = uintptr_t;

static const int kPageSize = 4096;

#endif  // types_h

