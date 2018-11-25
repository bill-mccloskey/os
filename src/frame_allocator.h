#ifndef frame_allocator_h
#define frame_allocator_h

#include "types.h"

class FrameAllocator {
public:
  FrameAllocator(phys_addr_t kernel_start_addr, phys_addr_t kernel_end_addr)
    : kernel_start_addr_(kernel_start_addr),
      kernel_end_addr_(kernel_end_addr) {}

  void AddRegion(phys_addr_t start_addr, phys_addr_t end_addr);

  phys_addr_t AllocateFrame();
  void FreeFrame(phys_addr_t frame);

private:
  struct Region {
    phys_addr_t start_addr;
    phys_addr_t end_addr;
  };

  static const int kMaxRegions = 32;
  Region regions_[kMaxRegions];
  int num_regions_ = 0;

  phys_addr_t kernel_start_addr_;
  phys_addr_t kernel_end_addr_;

  int cur_region_ = 0;
  phys_addr_t cur_addr_ = 0;

  // Free pages are linked together via a pointer at the start of the page.
  phys_addr_t free_pages_ = 0;
};

extern FrameAllocator* g_frame_allocator;

#endif
