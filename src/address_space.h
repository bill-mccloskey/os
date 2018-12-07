#ifndef address_space_h
#define address_space_h

#include "allocator.h"
#include "page_tables.h"
#include "refcount.h"
#include "types.h"

class Thread;

class AddressSpace : public RefCounted {
public:
  AddressSpace();

  Thread* CreateThread(virt_addr_t start_func, int priority);

  phys_addr_t table_root() const { return page_tables_.table_root(); }

  void Map(phys_addr_t phys_start, phys_addr_t phys_end,
           virt_addr_t virt_start, virt_addr_t virt_end,
           const PageAttributes& attrs);

  DECLARE_ALLOCATION_METHODS();

private:
  PageTableManager page_tables_;
};

extern Allocator<AddressSpace>* g_address_space_allocator;

#endif
