#include "address_space.h"
#include "frame_allocator.h"
#include "page_translation.h"
#include "thread.h"

static const virt_addr_t kStackBase = virt_addr_t(0x7ffffffff000);

AddressSpace::AddressSpace(bool kernel_space) : kernel_space_(kernel_space) {
  const size_t kMaxRAMSize = 1 << 21;
  page_tables_.Map(0, kMaxRAMSize, kKernelVirtualStart, kKernelVirtualStart + kMaxRAMSize, PageAttributes());
}

Thread* AddressSpace::CreateThread(virt_addr_t start_func, int priority) {
  const int kStackPages = 4;

  PageAttributes stack_attrs;
  for (int i = 0; i < kStackPages; i++) {
    phys_addr_t page = g_frame_allocator->AllocateFrame();
    virt_addr_t virt = kStackBase - (i + 1) * kPageSize;
    Map(page, page + kPageSize, virt, virt + kPageSize, stack_attrs);
  }

  PageAttributes guard_attrs;
  guard_attrs.set_present(false);
  virt_addr_t virt = kStackBase - (kStackPages + 1) * kPageSize;
  Map(0, 0, virt, virt + kPageSize, guard_attrs);

  return new Thread(start_func, kStackBase, RefPtr<AddressSpace>(this), priority, kernel_space_);
}

void AddressSpace::Map(phys_addr_t phys_start, phys_addr_t phys_end,
                       virt_addr_t virt_start, virt_addr_t virt_end,
                       const PageAttributes& attrs) {
  page_tables_.Map(phys_start, phys_end, virt_start, virt_end, attrs);
}

Allocator<AddressSpace>* g_address_space_allocator;
DEFINE_ALLOCATION_METHODS(AddressSpace, g_address_space_allocator);
