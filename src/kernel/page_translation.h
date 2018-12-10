#ifndef page_translation_h
#define page_translation_h

#include "assertions.h"
#include "types.h"

#ifdef TESTING

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

#else  // #ifdef TESTING

static const uintptr_t kKernelVirtualStart = 0xffff800000000000;

inline virt_addr_t PhysicalToVirtual(phys_addr_t phys) {
  assert_lt(phys, kKernelVirtualStart);
  return phys + kKernelVirtualStart;
}

inline phys_addr_t VirtualToPhysical(virt_addr_t virt) {
  assert_ge(virt, kKernelVirtualStart);
  return virt - kKernelVirtualStart;
}

#endif // TESTING

#endif // page_translation_h
