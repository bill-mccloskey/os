#include "frame_allocator.h"

#include "assertions.h"
#include "page_translation.h"

FrameAllocator* g_frame_allocator;

void FrameAllocator::AddRegion(phys_addr_t start_addr, phys_addr_t end_addr) {
  assert_eq(start_addr & (kPageSize - 1), 0);
  assert_eq(end_addr & (kPageSize - 1), 0);

  if (start_addr >= end_addr) return;

  // It's not a good idea to treat 0 as an allocated page.
  if (start_addr == 0) {
    start_addr += kPageSize;
    if (start_addr >= end_addr) return;
  }

  if (start_addr >= kernel_start_addr_ && start_addr < kernel_end_addr_) {
    start_addr = kernel_end_addr_;
    if (start_addr >= end_addr) return;
  }

  if (kernel_start_addr_ >= start_addr && kernel_start_addr_ < end_addr) {
    AddRegion(start_addr, kernel_start_addr_);
    AddRegion(kernel_end_addr_, end_addr);
    return;
  }

  regions_[num_regions_].start_addr = start_addr;
  regions_[num_regions_].end_addr = end_addr;

  if (num_regions_ == 0) {
    cur_addr_ = start_addr;
  }

  num_regions_++;
}

phys_addr_t FrameAllocator::AllocateFrame() {
  if (free_pages_) {
    phys_addr_t result = free_pages_;
    virt_addr_t virt = PhysicalToVirtual(free_pages_);
    free_pages_ = *((phys_addr_t *)virt);
    return result;
  }

  phys_addr_t result = cur_addr_;
  cur_addr_ += kPageSize;

  if (cur_addr_ == regions_[cur_region_].end_addr) {
    cur_region_++;
    if (cur_region_ == num_regions_) {
      panic("Out of memory");
    }

    cur_addr_ = regions_[cur_region_].start_addr;
  }

  return result;
}

void FrameAllocator::FreeFrame(phys_addr_t frame) {
  virt_addr_t virt = PhysicalToVirtual(frame);
  *((phys_addr_t *)virt) = free_pages_;
  free_pages_ = frame;
}