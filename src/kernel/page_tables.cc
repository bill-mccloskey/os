#include "page_tables.h"

#include "base/assertions.h"
#include "frame_allocator.h"
#include "page_translation.h"
#include "string.h"

static const uint64_t kPresent = 1 << 0;
static const uint64_t kWritable = 1 << 1;
static const uint64_t kUserAccessible = 1 << 2;
static const uint64_t kWriteThroughCaching = 1 << 3;
static const uint64_t kCachingDisabled = 1 << 4;
static const uint64_t kAccessed = 1 << 5;
static const uint64_t kDirty = 1 << 6;
static const uint64_t kLargerPage = 1 << 7;
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
  static const int kTableBits = 9;
  static const int kTableMask = (1 << kTableBits) - 1;
  static const int kNumTables = 4;

  assert_eq(phys_start & (kPageSize - 1), 0);
  assert_eq(phys_end & (kPageSize - 1), 0);
  assert_eq(virt_start & (kPageSize - 1), 0);
  assert_eq(virt_end & (kPageSize - 1), 0);

  if (attrs.present()) {
    assert_eq(phys_end - phys_start, virt_end - virt_start);
  }

  for (virt_addr_t virt = virt_start; virt < virt_end; ) {
    phys_addr_t phys = phys_start + (virt - virt_start);

    uint64_t page_size = kPageSize;
    int stop_level = 0;
    if (virt + kHugePageSize <= virt_end && (phys & (kHugePageSize - 1)) == 0 && (virt & (kHugePageSize - 1)) == 0) {
      // Use a huge page (1G).
      stop_level = 2;
      page_size = kHugePageSize;
    } else if (virt + kLargePageSize <= virt_end && (phys & (kLargePageSize - 1)) == 0 && (virt & (kLargePageSize - 1)) == 0) {
      // Use a large page (2M).
      stop_level = 1;
      page_size = kLargePageSize;
    }

    phys_addr_t table = table_;
    for (int i = kNumTables - 1; i >= stop_level; i--) {
      uint64_t* tablep = reinterpret_cast<uint64_t*>(PhysicalToVirtual(table));
      int entry_index = (virt >> (kPhysicalPageShift + i * kTableBits)) & kTableMask;
      assert_ge(entry_index, 0);
      assert_lt(entry_index, kPageSize / int(sizeof(uint64_t)));
      uint64_t* entryp = &tablep[entry_index];

      if (i == stop_level) {
        uint64_t flags = 0;
        if (stop_level > 0) flags |= kLargerPage;
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
          table = g_frame_allocator->AllocateFrame();
          ClearTable(table);
        }
        *entryp = table | kPresent | kWritable | kUserAccessible;
      }
    }

    virt += page_size;
  }
}
