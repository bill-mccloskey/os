#ifndef allocator_h
#define allocator_h

#include "frame_allocator.h"
#include "page_translation.h"
#include "types.h"

template<typename T>
class Allocator {
public:
  Allocator() {}

  T* Allocate() {
    PageFooter* p = *first_non_full_;
    if (p) {
      return AllocateFromPage(p);
    }

    phys_addr_t phys_page = g_frame_allocator->AllocateFrame();
    virt_addr_t page = PhysicalToVirtual(phys_page);
    p = reinterpret_cast<PageFooter*>(page + kFooterOffset);
    memset(p, 0, sizeof(PageFooter));
    *first_non_full_ = p;
    return AllocateFromPage(p);
  }

  void Deallocate(T* ptr) {
    virt_addr_t addr = reinterpret_cast<virt_addr_t>(ptr);
    virt_addr_t base = addr & ~(kPageSize - 1);
    PageFooter* page = reinterpret_cast<PageFooter*>(base + kFooterOffset);

    int index = (addr - base) / kAllocationSize;
    int byte = index / 8;
    int bit = index % 8;
    page->allocated_map[byte] &= ~(1 << bit);

    page->num_allocated--;

    ReSortPages(page);
  }

private:
  static const size_t kMinSize = sizeof(uint64_t);
  static_assert(sizeof(T) >= kMinSize);

  static constexpr size_t AllocationSize() {
    size_t sz = sizeof(T);

    // Compute the next power of two >= sz.
    sz--;
    sz |= sz >> 1;
    sz |= sz >> 2;
    sz |= sz >> 4;
    sz |= sz >> 8;
    sz |= sz >> 16;
    sz |= sz >> 32;
    sz++;

    return sz;
  }

  static const size_t kBitmapSize = kPageSize / kMinSize / 8;

  struct PageFooter {
    PageFooter* next;
    size_t num_allocated;
    char allocated_map[kBitmapSize];
  };

  int FindFreeIndex(const PageFooter* footer) {
    assert(footer->num_allocated < kObjectsPerPage);

    const char* p = &footer->allocated_map[0];
    for (int i = 0; i < kBitmapSize; i++) {
      char bits = *p;
      if (bits == 0xff) continue;

      if (bits & (1 << 0) == 0) return i * 8 + 0;
      if (bits & (1 << 1) == 0) return i * 8 + 1;
      if (bits & (1 << 2) == 0) return i * 8 + 2;
      if (bits & (1 << 3) == 0) return i * 8 + 3;
      if (bits & (1 << 4) == 0) return i * 8 + 4;
      if (bits & (1 << 5) == 0) return i * 8 + 5;
      if (bits & (1 << 6) == 0) return i * 8 + 6;
      if (bits & (1 << 7) == 0) return i * 8 + 7;
    }
  }

  T* AllocateFromPage(PageFooter* footer) {
    int index = FindFreeIndex(footer);

    int byte = index / 8;
    int bit = index % 8;
    footer->allocated_map[byte] |= 1 << bit;

    int num_allocated = footer->num_allocated++;
    if (num_allocated == kObjectsPerPage) {
      *first_non_full_ = &footer->next;
    }

    virt_addr_t base = reinterpret_cast<virt_addr_t>(footer) & ~(kPageSize - 1);
    T* ptr = reinterpret_cast<T*>(base + index * kAllocationSize);

    return ptr;
  }

  void ReSortPages(PageFooter* footer) {
    
  }

  static const size_t kAllocationSize = AllocationSize();
  static const int kObjectsPerPage = (kPageSize - sizeof(PageFooter)) / kAllocationSize;
  static const int kFooterOffset = kPageSize - sizeof(PageFooter);

  // Pages sorted by num_allocated, with the most full earlier in the list.
  PageFooter* sorted_pages_ = nullptr;

  // Pointer to the middle of sorted_pages_. Points to the first non-full page.
  PageFooter** first_non_full_ = nullptr;
};

#endif
