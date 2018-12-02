#ifndef allocator_h
#define allocator_h

#include "frame_allocator.h"
#include "linked_list.h"
#include "page_translation.h"
#include "types.h"

template<typename T>
class Allocator {
public:
  Allocator() {}

  T* Allocate() {
    if (free_list_.IsEmpty()) {
      phys_addr_t phys_page = g_frame_allocator->AllocateFrame();
      virt_addr_t page = PhysicalToVirtual(phys_page);
      ClearPage(page);
    }

    FreeObject* obj = free_list_.PopFront();
    return reinterpret_cast<T*>(obj);
  }

  void Deallocate(T* ptr) {
    FreeObject* free = new (ptr) FreeObject;
    free_list_.PushFront(free->entry);

    virt_addr_t addr = reinterpret_cast<virt_addr_t>(ptr);
    virt_addr_t base = addr & ~(kPageSize - 1);
    PageFooter* page = reinterpret_cast<PageFooter*>(base + kFooterOffset);

    page->num_allocated--;
    if (page->num_allocated > 0) {
      return;
    }

    virt_addr_t end = base + kFooterOffset;
    for (virt_addr_t addr = base; addr < end; addr += kAllocationSize) {
      FreeObject* free = reinterpret_cast<FreeObject*>(addr);
      free->entry.Remove();
    }

    g_frame_allocator->FreeFrame(base);
  }

  static const size_t kAllocationSize = sizeof(T);

private:
  struct PageFooter {
    size_t num_allocated;
  };

  struct FreeObject {
    LinkedListEntry entry;
  };

  static const size_t kMinSize = sizeof(FreeObject);
  static_assert(sizeof(T) >= kMinSize);

  void ClearPage(virt_addr_t virt) {
    virt_addr_t end = virt + kObjectsPerPage * kAllocationSize;
    for (virt_addr_t addr = virt; addr < end; addr += kAllocationSize) {
      FreeObject* free = new (reinterpret_cast<void*>(addr)) FreeObject;
      free_list_.PushFront(free->entry);
    }

    PageFooter* footer = reinterpret_cast<PageFooter*>(virt + kFooterOffset);
    footer->num_allocated = 0;
  }

  static const int kObjectsPerPage = (kPageSize - sizeof(PageFooter)) / kAllocationSize;
  static const int kFooterOffset = kPageSize - sizeof(PageFooter);

  using FreeList = LINKED_LIST(FreeObject, entry);
  FreeList free_list_;
};

#endif
