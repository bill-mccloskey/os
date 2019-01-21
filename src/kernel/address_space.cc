#include "address_space.h"

#include "kernel/frame_allocator.h"
#include "kernel/page_translation.h"
#include "kernel/thread.h"

#include <string.h>

static const virt_addr_t kStackBase = virt_addr_t(0x7ffffffff000);

AddressSpace::AddressSpace() {
  const size_t kMaxRAMSize = 64 * (uint64_t(1) << 30);
  page_tables_.Map(0, kMaxRAMSize, g_kernel_virtual_start, g_kernel_virtual_start + kMaxRAMSize, PageAttributes());
}

Thread* AddressSpace::CreateThread(virt_addr_t start_func, int priority,
                                   void* stack_data, size_t stack_data_len) {
  const int kStackPages = 4;

  PageAttributes stack_attrs;
  stack_attrs.set_no_execute(true);
  phys_addr_t top_stack_page;
  for (int i = 0; i < kStackPages; i++) {
    phys_addr_t page = g_frame_allocator->AllocateFrame();
    virt_addr_t virt = kStackBase - (i + 1) * kPageSize;
    Map(page, page + kPageSize, virt, virt + kPageSize, stack_attrs);

    if (i == 0) {
      top_stack_page = page;
    }
  }

  virt_addr_t stack_base = kStackBase;
  if (stack_data_len) {
    assert_lt(stack_data_len, kPageSize);
    stack_base -= stack_data_len;

    virt_addr_t vstack_base = PhysicalToVirtual(top_stack_page + kPageSize - stack_data_len);
    memcpy(reinterpret_cast<char*>(vstack_base), stack_data, stack_data_len);
  }

  PageAttributes guard_attrs;
  guard_attrs.set_present(false);
  virt_addr_t virt = kStackBase - (kStackPages + 1) * kPageSize;
  Map(0, 0, virt, virt + kPageSize, guard_attrs);

  return new Thread(start_func, stack_base, RefPtr<AddressSpace>(this), priority);
}

void AddressSpace::Map(phys_addr_t phys_start, phys_addr_t phys_end,
                       virt_addr_t virt_start, virt_addr_t virt_end,
                       const PageAttributes& attrs) {
  page_tables_.Map(phys_start, phys_end, virt_start, virt_end, attrs);
}

Allocator<AddressSpace>* g_address_space_allocator;
DEFINE_ALLOCATION_METHODS(AddressSpace, g_address_space_allocator);
