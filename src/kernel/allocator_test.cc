#include "allocator.h"
#include "page_translation.h"

#include "gtest/gtest.h"

#include <map>
#include <stdio.h>
#include <sys/mman.h>

uintptr_t g_kernel_virtual_start = 0;
intptr_t g_kernel_virtual_offset = (1 << 21);

struct SimpleObject {
  uint64_t a = 0;
  uint64_t b = 0;
};

TEST(AllocatorTest, Basic) {
  Allocator<SimpleObject> alloc;
  SimpleObject* obj1 = alloc.Allocate();
  SimpleObject* obj2 = alloc.Allocate();
  EXPECT_NE(obj1, obj2);
  obj1->a = 10;
  obj1->b = 20;
  obj2->a = 30;
  obj2->b = 40;
  alloc.Deallocate(obj1);
  alloc.Deallocate(obj2);
}

TEST(AllocatorTest, Random) {
  Allocator<SimpleObject> alloc;
  std::map<uintptr_t, int> allocations;

  int32_t seed = testing::FLAGS_gtest_random_seed;
  srandom(seed);

  for (int i = 0; i < 1000000; i++) {
    int r = random() % 3;
    if (r == 0) {
      SimpleObject* obj = alloc.Allocate();
      obj->a = obj->b = i;
      uintptr_t addr = reinterpret_cast<uintptr_t>(obj);
      uintptr_t offset = addr & (kPageSize - 1);
      EXPECT_EQ(offset % decltype(alloc)::kAllocationSize, 0);
      auto result = allocations.insert(std::make_pair(addr, i));
      EXPECT_TRUE(result.second);
    } else if (r == 1) {
      if (allocations.empty()) continue;
      auto iter = allocations.begin();
      std::advance(iter, random() % allocations.size());
      auto kv = *iter;
      SimpleObject* obj = reinterpret_cast<SimpleObject*>(kv.first);
      EXPECT_EQ(obj->a, kv.second);
      EXPECT_EQ(obj->b, kv.second);
      alloc.Deallocate(obj);
      allocations.erase(iter);
    } else {
      if (allocations.empty()) continue;
      auto iter = allocations.begin();
      std::advance(iter, random() % allocations.size());
      auto kv = *iter;
      SimpleObject* obj = reinterpret_cast<SimpleObject*>(kv.first);
      EXPECT_EQ(obj->a, kv.second);
      EXPECT_EQ(obj->b, kv.second);
    }
  }
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
