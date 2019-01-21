#ifndef page_translation_h
#define page_translation_h

#include "base/assertions.h"
#include "base/types.h"

extern uintptr_t g_kernel_virtual_start;
extern intptr_t g_kernel_virtual_offset;

inline virt_addr_t PhysicalToVirtual(phys_addr_t phys) {
  assert_lt(phys, g_kernel_virtual_start);
  return phys + g_kernel_virtual_offset;
}

inline phys_addr_t VirtualToPhysical(virt_addr_t virt) {
  assert_ge(virt, g_kernel_virtual_start);
  return virt - g_kernel_virtual_offset;
}

#endif // page_translation_h
