#ifndef page_translation_h
#define page_translation_h

#include "types.h"

static const uintptr_t kKernelVirtualStart = 0xffff800000000000;

inline virt_addr_t PhysicalToVirtual(phys_addr_t phys) {
  // assert_lt(phys, kKernelVirtualStart);
  return phys + kKernelVirtualStart;
}

inline phys_addr_t VirtualToPhysical(virt_addr_t virt) {
  // assert_ge(virt, kKernelVirtualStart);
  return virt - kKernelVirtualStart;
}

#endif // page_translation_h
