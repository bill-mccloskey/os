#include "frame_allocator.h"
#include "page_tables.h"
#include "page_translation.h"

#include "gtest/gtest.h"

#include <map>
#include <stdio.h>
#include <sys/mman.h>

uintptr_t g_kernel_virtual_start = 0;
intptr_t g_kernel_virtual_offset = (1 << 21);

static virt_addr_t MakeAddressForTables(uint64_t tab1, uint64_t tab2, uint64_t tab3, uint64_t tab4) {
  EXPECT_EQ(tab1 & ~0x1ff, 0u);
  EXPECT_EQ(tab2 & ~0x1ff, 0u);
  EXPECT_EQ(tab3 & ~0x1ff, 0u);
  EXPECT_EQ(tab4 & ~0x1ff, 0u);

  virt_addr_t addr = (((tab1 << (9*3)) + (tab2 << (9*2)) + (tab3 << (9*1)) + (tab4 << (9*0))) << 12);
  if (addr & (virt_addr_t(1) << 47)) addr |= ~virt_addr_t((virt_addr_t(1) << 48) - 1);
  return addr;
}

static uint64_t GetEntry(phys_addr_t table, int entry) {
  EXPECT_EQ(entry & ~0x1ff, 0);
  virt_addr_t virt = PhysicalToVirtual(table);
  return reinterpret_cast<uint64_t*>(virt)[entry];
}

static bool GetBit(uint64_t entry, int bit) {
  return entry & (uint64_t(1) << bit) ? true : false;
}

static phys_addr_t ReadEntry(uint64_t entry, int level, int stop_level, const PageAttributes& attrs) {
  bool present = GetBit(entry, 0);
  bool rw = GetBit(entry, 1);
  bool us = GetBit(entry, 2);
  bool pwt = GetBit(entry, 3);
  bool pcd = GetBit(entry, 4);
  bool a = GetBit(entry, 5);
  bool nx = GetBit(entry, 63);

  if (level == 3) {
    EXPECT_EQ(present, attrs.present());
    EXPECT_EQ(rw, attrs.writable());
    EXPECT_EQ(us, attrs.user_accessible());
    EXPECT_EQ(nx, attrs.no_execute());
  } else {
    if (attrs.present()) EXPECT_TRUE(present);
    if (attrs.writable()) EXPECT_TRUE(rw);
    if (attrs.user_accessible()) EXPECT_TRUE(us);
    if (!attrs.no_execute()) EXPECT_FALSE(nx);
  }

  EXPECT_EQ(pwt, false);
  EXPECT_EQ(pcd, false);
  EXPECT_EQ(a, false);

  EXPECT_EQ(GetBit(entry, 9), 0);
  EXPECT_EQ(GetBit(entry, 10), 0);
  EXPECT_EQ(GetBit(entry, 11), 0);

  phys_addr_t phys = ((entry >> 12) & ((uint64_t(1) << 40) - 1)) << 12;

  if (level == stop_level) {
    bool d = GetBit(entry, 6);
    bool g = GetBit(entry, 8);

    EXPECT_EQ(d, false);
    EXPECT_EQ(g, attrs.global());

    if (stop_level == 3) {
      bool pat = GetBit(entry, 7);
      EXPECT_EQ(pat, false);
    } else {
      // This is the bit for larger pages.
      EXPECT_TRUE(GetBit(entry, 7));
    }

    if (stop_level == 2) {
      EXPECT_EQ(phys & (kLargePageSize - 1), 0ul);
    } else if (stop_level == 1) {
      EXPECT_EQ(phys & (kHugePageSize - 1), 0ul);
    }
  } else {
    EXPECT_EQ(GetBit(entry, 6), 0);
    EXPECT_EQ(GetBit(entry, 7), 0);
    EXPECT_EQ(GetBit(entry, 8), 0);
  }

  // I think this is where the PCID would go.
  EXPECT_EQ((entry >> 52) & ((1 << 10) - 1), 0ul);

  return phys;
}

TEST(PageTablesTest, Basic) {
  PageTableManager tables;

  PageAttributes attrs;

  phys_addr_t phys = kPageSize * 2;
  virt_addr_t virt = MakeAddressForTables(38, 147, 22, 418);
  tables.Map(phys, phys + kPageSize,
             virt, virt + kPageSize,
             attrs);

  phys_addr_t entry = tables.table_root();
  entry = ReadEntry(GetEntry(entry, 38), 0, 3, attrs);
  entry = ReadEntry(GetEntry(entry, 147), 1, 3, attrs);
  entry = ReadEntry(GetEntry(entry, 22), 2, 3, attrs);
  entry = ReadEntry(GetEntry(entry, 418), 3, 3, attrs);
  EXPECT_EQ(entry, phys);
}

// Large pages must start at virtually aligned addresses.
TEST(PageTablesTest, NotLargePages1) {
  PageTableManager tables;

  PageAttributes attrs;

  phys_addr_t phys = kLargePageSize - kPageSize * 2;
  virt_addr_t virt = MakeAddressForTables(38, 147, 22, 418);
  tables.Map(phys, phys + kLargePageSize + kPageSize * 2,
             virt, virt + kLargePageSize + kPageSize * 2,
             attrs);

  phys_addr_t entry = tables.table_root();
  entry = ReadEntry(GetEntry(entry, 38), 0, 3, attrs);
  entry = ReadEntry(GetEntry(entry, 147), 1, 3, attrs);
  entry = ReadEntry(GetEntry(entry, 22), 2, 3, attrs);
  entry = ReadEntry(GetEntry(entry, 419), 3, 3, attrs);
  EXPECT_EQ(entry, phys + kPageSize);

  entry = tables.table_root();
  entry = ReadEntry(GetEntry(entry, 38), 0, 3, attrs);
  entry = ReadEntry(GetEntry(entry, 147), 1, 3, attrs);
  entry = ReadEntry(GetEntry(entry, 22), 2, 3, attrs);
  entry = ReadEntry(GetEntry(entry, 420), 3, 3, attrs);
  EXPECT_EQ(entry, phys + 2 * kPageSize);
}

// Large pages must start at physically aligned addresses.
TEST(PageTablesTest, NotLargePages2) {
  PageTableManager tables;

  PageAttributes attrs;

  phys_addr_t phys = kLargePageSize - kPageSize;
  virt_addr_t virt = MakeAddressForTables(38, 147, 22, 0);
  tables.Map(phys, phys + kLargePageSize + kPageSize,
             virt, virt + kLargePageSize + kPageSize,
             attrs);

  phys_addr_t entry = tables.table_root();
  entry = ReadEntry(GetEntry(entry, 38), 0, 3, attrs);
  entry = ReadEntry(GetEntry(entry, 147), 1, 3, attrs);
  entry = ReadEntry(GetEntry(entry, 22), 2, 3, attrs);
  entry = ReadEntry(GetEntry(entry, 0), 3, 3, attrs);
  EXPECT_EQ(entry, phys);

  entry = tables.table_root();
  entry = ReadEntry(GetEntry(entry, 38), 0, 3, attrs);
  entry = ReadEntry(GetEntry(entry, 147), 1, 3, attrs);
  entry = ReadEntry(GetEntry(entry, 22), 2, 3, attrs);
  entry = ReadEntry(GetEntry(entry, 1), 3, 3, attrs);
  EXPECT_EQ(entry, phys + kPageSize);
}

TEST(PageTablesTest, LargePages) {
  PageTableManager tables;

  PageAttributes attrs;

  phys_addr_t phys = kLargePageSize - kPageSize;
  virt_addr_t virt = MakeAddressForTables(38, 147, 22, 511);
  tables.Map(phys, phys + kLargePageSize + 2 * kPageSize,
             virt, virt + kLargePageSize + 2 * kPageSize,
             attrs);

  phys_addr_t entry = tables.table_root();
  entry = ReadEntry(GetEntry(entry, 38), 0, 3, attrs);
  entry = ReadEntry(GetEntry(entry, 147), 1, 3, attrs);
  entry = ReadEntry(GetEntry(entry, 22), 2, 3, attrs);
  entry = ReadEntry(GetEntry(entry, 511), 3, 3, attrs);
  EXPECT_EQ(entry, phys);

  entry = tables.table_root();
  entry = ReadEntry(GetEntry(entry, 38), 0, 2, attrs);
  entry = ReadEntry(GetEntry(entry, 147), 1, 2, attrs);
  entry = ReadEntry(GetEntry(entry, 23), 2, 2, attrs);
  EXPECT_EQ(entry, phys + kPageSize);
}

TEST(PageTablesTest, HugePages) {
  PageTableManager tables;

  PageAttributes attrs;

  phys_addr_t phys = kHugePageSize - kLargePageSize;
  virt_addr_t virt = MakeAddressForTables(38, 147, 511, 0);
  tables.Map(phys, phys + kHugePageSize + kLargePageSize,
             virt, virt + kHugePageSize + kLargePageSize,
             attrs);

  phys_addr_t entry = tables.table_root();
  entry = ReadEntry(GetEntry(entry, 38), 0, 2, attrs);
  entry = ReadEntry(GetEntry(entry, 147), 1, 2, attrs);
  entry = ReadEntry(GetEntry(entry, 511), 2, 2, attrs);
  EXPECT_EQ(entry, phys);

  entry = tables.table_root();
  entry = ReadEntry(GetEntry(entry, 38), 0, 1, attrs);
  entry = ReadEntry(GetEntry(entry, 148), 1, 1, attrs);
  EXPECT_EQ(entry, phys + kLargePageSize);
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);

  void* region = mmap(nullptr, 1 << 20, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  assert_ne(region, MAP_FAILED);
  memset(region, 0xcd, 1 << 20);

  virt_addr_t virt = reinterpret_cast<virt_addr_t>(region);
  g_kernel_virtual_start = virt;

  phys_addr_t phys = VirtualToPhysical(virt);

  FrameAllocator frame_alloc(0, 0, 0, 0);
  frame_alloc.AddRegion(phys, phys + (1 << 20));
  g_frame_allocator = &frame_alloc;

  return RUN_ALL_TESTS();
}
