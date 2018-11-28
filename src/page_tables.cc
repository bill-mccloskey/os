#include "page_tables.h"

#include "assert.h"
#include "frame_allocator.h"
#include "page_translation.h"
#include "serial.h"
#include "string.h"

static const uint64_t kPresent = 1 << 0;
static const uint64_t kWritable = 1 << 1;
static const uint64_t kUserAccessible = 1 << 2;
static const uint64_t kWriteThroughCaching = 1 << 3;
static const uint64_t kCachingDisabled = 1 << 4;
static const uint64_t kAccessed = 1 << 5;
static const uint64_t kDirty = 1 << 6;
static const uint64_t kLargePage = 1 << 7;
static const uint64_t kGlobalPage = 1 << 8; // Only for 4K leaf entries
static const uint64_t kNoExecute = uint64_t(1) << 63;

static const uint64_t kPhysicalPageShift = 12;
static const uint64_t kPageTableBits = 40;

PageTableManager::PageTableManager() {
  table_ = g_frame_allocator->AllocateFrame();
  ClearTable(table_);
}

void PageTableManager::ClearTable(phys_addr_t table) {
  char* p = reinterpret_cast<char*>(PhysicalToVirtual(table));
  memset(p, 0, kPageSize);
}

void PageTableManager::Map(phys_addr_t phys_start, phys_addr_t phys_end,
                           virt_addr_t virt_start, virt_addr_t virt_end,
                           const PageAttributes& attrs) {
  g_serial->Printf("Map(phys=[%p, %p), virt=[%p, %p))\n", phys_start, phys_end, virt_start, virt_end);

  static const int kTableBits = 9;
  static const int kTableMask = (1 << kTableBits) - 1;
  static const int kNumTables = 4;

  assert_eq(phys_start & (kPageSize - 1), 0);
  assert_eq(phys_end & (kPageSize - 1), 0);
  assert_eq(virt_start & (kPageSize - 1), 0);
  assert_eq(virt_end & (kPageSize - 1), 0);

  for (virt_addr_t virt = virt_start; virt < virt_end; virt += kPageSize) {
    phys_addr_t phys = phys_start + (virt - virt_start);

    phys_addr_t table = table_;
    for (int i = kNumTables - 1; i >= 0; i--) {
      g_serial->Printf("  table = %p\n", table);
      uint64_t* tablep = reinterpret_cast<uint64_t*>(PhysicalToVirtual(table));
      int entry_index = (virt >> (kPhysicalPageShift + i * kTableBits)) & kTableMask;
      g_serial->Printf("  entry_index = %d\n", entry_index);
      assert_ge(entry_index, 0);
      assert_lt(entry_index, kPageSize / int(sizeof(uint64_t)));
      uint64_t* entryp = &tablep[entry_index];

      if (i == 0) {
        uint64_t flags = 0;
        flags |= attrs.present() ? kPresent : 0;
        flags |= attrs.writable() ? kWritable : 0;
        flags |= attrs.user_accessible() ? kUserAccessible : 0;
        flags |= attrs.global() ? kGlobalPage : 0;
        flags |= attrs.no_execute() ? kNoExecute : 0;
        *entryp = phys | flags;
      } else {
        if (*entryp & kPresent) {
          table = ((*entryp >> kPhysicalPageShift) & ((uint64_t(1) << kPageTableBits) - 1)) << kPhysicalPageShift;
        } else {
          phys_addr_t new_table = g_frame_allocator->AllocateFrame();
          ClearTable(new_table);
          *entryp = new_table | kPresent | kWritable | kUserAccessible;
          table = new_table;
        }
      }
    }
  }
}
