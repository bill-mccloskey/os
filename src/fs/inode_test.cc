#include "fs/inode.h"
#include "base/assertions.h"
#include "base/output_stream.h"

#include "gtest/gtest.h"

class TestBlockHandle : public BlockHandle {
public:
  TestBlockHandle(block_number_t number, char* data)
    : number_(number), data_(data) {
    //LOG(INFO) << "Fetch block " << number_;
  }

  block_number_t block_number() const override { return number_; }
  char* Get() override {
    //LOG(INFO) << "Get block " << number_;
    return data_;
  }
  void SetDirty() override {
    //LOG(INFO) << "Dirty block " << number_;
  }
  void SyncNow() override {}
  void Release() override {
    //LOG(INFO) << "Release block " << number_;
    delete this;
  }

private:
  block_number_t number_;
  char* data_;
};

static const int kBlockSize = 128;
static const int kMaxBlocks = 512;

class TestEnv : public FileSystemEnv {
public:
  ErrorCode FetchBlock(block_number_t block, BlockHandle** result) override MUST_USE {
    assert_lt(block, kMaxBlocks);
    *result = new TestBlockHandle(block, blocks[block]);
    return ErrorCode::kOk;
  }

  ErrorCode FreeBlock(block_number_t block) override {
    return ErrorCode::kOk;
  }

  uint64_t Now() override { return 10; }

private:
  char blocks[kMaxBlocks][kBlockSize];
};

class InodeTest : public testing::Test {
protected:
  void DumpBlocks(InodeHandle* h);
  void DumpBlocksRecursive(uint64_t block_index, uint64_t block_count,
                           block_number_t* tree, uint64_t tree_size,
                           uint64_t subtrees_per_block);

  void CollectBlocks(uint64_t block_index, uint64_t block_count,
                     block_number_t* tree, uint64_t tree_size,
                     uint64_t subtrees_per_block,
                     std::map<block_number_t, int>& blocks);
  void CollectBlocks(FileSystem* fs, InodeHandle* h, std::map<block_number_t, int>& blocks);

  TestEnv env_;
};

TEST_F(InodeTest, Basic) {
  FileSystem* fs;
  ASSERT_EQ(FileSystem::Create(&env_, /*block_size=*/ kBlockSize, /*num_blocks=*/ 16, /*max_inodes=*/ 2, &fs), ErrorCode::kOk);

  InodeHandle* h1;
  ASSERT_EQ(fs->AllocateInode(&h1), ErrorCode::kOk);
  ASSERT_EQ(h1->TEST_GetBlock()->block_number(), 3ul);

  ASSERT_EQ(h1->Length(), 0ul);
  block_number_t num;
  ASSERT_EQ(h1->GetDataBlock(0, &num), ErrorCode::kFileTooSmall);

  ASSERT_EQ(h1->SetLength(10), ErrorCode::kOk);
  ASSERT_EQ(h1->GetDataBlock(0, &num), ErrorCode::kOk);
  ASSERT_EQ(num, 5ul);

  InodeHandle* h2;
  ASSERT_EQ(fs->AllocateInode(&h2), ErrorCode::kOk);
  ASSERT_EQ(h2->TEST_GetBlock()->block_number(), 4ul);

  fs->Unmount();
  delete fs;
}

TEST_F(InodeTest, MultiBlockFile) {
  FileSystem* fs;
  ASSERT_EQ(FileSystem::Create(&env_, /*block_size=*/ kBlockSize, /*num_blocks=*/ 16, /*max_inodes=*/ 2, &fs), ErrorCode::kOk);

  InodeHandle* h1;
  ASSERT_EQ(fs->AllocateInode(&h1), ErrorCode::kOk);

  ASSERT_EQ(h1->SetLength(kBlockSize*3 + kBlockSize/2), ErrorCode::kOk);
  block_number_t num;
  ASSERT_EQ(h1->GetDataBlock(0, &num), ErrorCode::kOk);
  ASSERT_EQ(num, 5ul);
  ASSERT_EQ(h1->GetDataBlock(1, &num), ErrorCode::kOk);
  ASSERT_EQ(num, 6ul);
  ASSERT_EQ(h1->GetDataBlock(2, &num), ErrorCode::kOk);
  ASSERT_EQ(num, 7ul);
  ASSERT_EQ(h1->GetDataBlock(3, &num), ErrorCode::kOk);
  ASSERT_EQ(num, 8ul);

  fs->Unmount();
  delete fs;
}

void InodeTest::DumpBlocksRecursive(uint64_t block_index, uint64_t block_count,
                                    block_number_t* tree, uint64_t tree_size,
                                    uint64_t subtrees_per_block) {
  LOG(INFO) << "DBR " << block_index << " " << block_count << " " << tree_size;

  while (block_index < block_count) {
    if (tree_size == 1) {
      LOG(INFO) << "[1] " << *tree;
    } else {
      block_number_t metablock = *tree;
      LOG(INFO) << "[" << tree_size << "] " << *tree;
      BlockHandle* h;
      ErrorCode e = env_.FetchBlock(metablock, &h);
      ASSERT_EQ(e, ErrorCode::kOk);
      block_number_t* subtree = reinterpret_cast<block_number_t*>(h->Get());
      uint64_t max_block = block_index + tree_size;
      if (max_block > block_count) max_block = block_count;
      DumpBlocksRecursive(block_index, max_block, subtree, tree_size / subtrees_per_block, subtrees_per_block);
      h->Release();
    }

    tree++;
    block_index += tree_size;
  }
}

static uint64_t RoundUp(uint64_t numerator, uint64_t denominator) {
  return (numerator + (denominator - 1)) / denominator;
}

void InodeTest::DumpBlocks(InodeHandle* h) {
  block_number_t* trees = h->TEST_GetTrees();
  uint64_t num_blocks = RoundUp(h->Length(), kBlockSize);
  uint64_t tree_size = h->TEST_GetTreeSizeInBlocks();

  DumpBlocksRecursive(0, num_blocks, trees, tree_size, h->SubtreesPerBlock());
}

TEST_F(InodeTest, LargeFileCreate1) {
  FileSystem* fs;
  ASSERT_EQ(FileSystem::Create(&env_, /*block_size=*/ kBlockSize, /*num_blocks=*/ 256, /*max_inodes=*/ 2, &fs), ErrorCode::kOk);

  InodeHandle* h1;
  ASSERT_EQ(fs->AllocateInode(&h1), ErrorCode::kOk);

  ASSERT_EQ(h1->SetLength(kBlockSize * 190), ErrorCode::kOk);

  block_number_t num;
  ASSERT_EQ(h1->GetDataBlock(0, &num), ErrorCode::kOk);
  ASSERT_EQ(num, 5ul);
  ASSERT_EQ(h1->GetDataBlock(16, &num), ErrorCode::kOk);
  ASSERT_EQ(num, 23ul);
  ASSERT_EQ(h1->GetDataBlock(176, &num), ErrorCode::kOk);
  ASSERT_EQ(num, 193ul);

  fs->Unmount();
  delete fs;
}

TEST_F(InodeTest, LargeFileCreate2) {
  FileSystem* fs;
  ASSERT_EQ(FileSystem::Create(&env_, /*block_size=*/ kBlockSize, /*num_blocks=*/ 256, /*max_inodes=*/ 2, &fs), ErrorCode::kOk);

  InodeHandle* h1;
  ASSERT_EQ(fs->AllocateInode(&h1), ErrorCode::kOk);

  ASSERT_EQ(h1->SetLength(kBlockSize * 200), ErrorCode::kOk);

  block_number_t num;
  ASSERT_EQ(h1->GetDataBlock(0, &num), ErrorCode::kOk);
  ASSERT_EQ(num, 5ul);
  ASSERT_EQ(h1->GetDataBlock(16, &num), ErrorCode::kOk);
  ASSERT_EQ(num, 24ul);
  ASSERT_EQ(h1->GetDataBlock(192, &num), ErrorCode::kOk);
  ASSERT_EQ(num, 211ul);

  fs->Unmount();
  delete fs;
}

TEST_F(InodeTest, LargeFileDelete) {
  FileSystem* fs;
  ASSERT_EQ(FileSystem::Create(&env_, /*block_size=*/ kBlockSize, /*num_blocks=*/ 256, /*max_inodes=*/ 2, &fs), ErrorCode::kOk);

  InodeHandle* h1;
  ASSERT_EQ(fs->AllocateInode(&h1), ErrorCode::kOk);

  ASSERT_EQ(h1->SetLength(kBlockSize * 200), ErrorCode::kOk);
  DumpBlocks(h1);

  ASSERT_EQ(h1->SetLength(kBlockSize * 3), ErrorCode::kOk);
  DumpBlocks(h1);

  block_number_t num;
  ASSERT_EQ(h1->GetDataBlock(0, &num), ErrorCode::kOk);
  ASSERT_EQ(num, 5ul);
  ASSERT_EQ(h1->GetDataBlock(1, &num), ErrorCode::kOk);
  ASSERT_EQ(num, 6ul);
  ASSERT_EQ(h1->GetDataBlock(2, &num), ErrorCode::kOk);
  ASSERT_EQ(num, 7ul);

  for (block_number_t i = 8; i < 218; i++) {
    ASSERT_FALSE(fs->TEST_IsBlockAllocated(i)) << "(block " << i << ")";
  }

  fs->Unmount();
  delete fs;
}

void InodeTest::CollectBlocks(uint64_t block_index, uint64_t block_count,
                              block_number_t* tree, uint64_t tree_size,
                              uint64_t subtrees_per_block,
                              std::map<block_number_t, int>& blocks) {
  while (block_index < block_count) {
    auto r = blocks.insert(std::make_pair(*tree, tree_size));
    ASSERT_TRUE(r.second);

    if (tree_size > 1) {
      block_number_t metablock = *tree;
      BlockHandle* h;
      ErrorCode e = env_.FetchBlock(metablock, &h);
      ASSERT_EQ(e, ErrorCode::kOk);
      block_number_t* subtree = reinterpret_cast<block_number_t*>(h->Get());
      uint64_t max_block = block_index + tree_size;
      if (max_block > block_count) max_block = block_count;
      CollectBlocks(block_index, max_block, subtree, tree_size / subtrees_per_block, subtrees_per_block, blocks);
      h->Release();
    }

    tree++;
    block_index += tree_size;
  }
}

void InodeTest::CollectBlocks(FileSystem* fs, InodeHandle* h, std::map<block_number_t, int>& blocks) {
  block_number_t* trees = h->TEST_GetTrees();
  uint64_t num_blocks = RoundUp(h->Length(), kBlockSize);
  uint64_t tree_size = h->TEST_GetTreeSizeInBlocks();
  CollectBlocks(0, num_blocks, trees, tree_size, h->SubtreesPerBlock(), blocks);
}

TEST_F(InodeTest, Randomized) {
  int32_t seed = testing::FLAGS_gtest_random_seed;
  srandom(seed);

  FileSystem* fs;
  ASSERT_EQ(FileSystem::Create(&env_, /*block_size=*/ kBlockSize, /*num_blocks=*/ 256, /*max_inodes=*/ 2, &fs), ErrorCode::kOk);

  InodeHandle* h1;
  ASSERT_EQ(fs->AllocateInode(&h1), ErrorCode::kOk);

  InodeHandle* h2;
  ASSERT_EQ(fs->AllocateInode(&h2), ErrorCode::kOk);

  // With 256 max blocks, we need 5 for the initial bitmaps and inode tables.
  // Then we need 15 size-16 subtrees and 1 size-256 subtree.
  uint64_t max_length = 235 * kBlockSize;

  // Tests with one inode.
  for (int i = 0; i < 10000; i++) {
    LOG(INFO) << "=== Iteration " << i << " ===";
    size_t length = random() % (kBlockSize * 300);
    LOG(INFO) << "=== Setting length to " << length << " ===";
    ErrorCode expected = length > max_length ? ErrorCode::kDataBlocksExhausted : ErrorCode::kOk;
    ASSERT_EQ(h1->SetLength(length), expected) << "len=" << length;
    if (expected == ErrorCode::kOk) {
      ASSERT_EQ(h1->Length(), length);
      ASSERT_EQ(fs->TEST_NumDataBlocksUsed(), fs->ComputeTotalBlockUsage(length));
    }
    LOG(INFO) << "=== Dump ===";
    //DumpBlocks(h1);

    std::map<block_number_t, int> blocks;
    CollectBlocks(fs, h1, blocks);

    for (block_number_t b = 5; b < 256; b++) {
      bool is_allocated = fs->TEST_IsBlockAllocated(b);
      bool is_found = blocks.count(b) > 0;
      ASSERT_EQ(is_allocated, is_found) << "(block " << b << ")";
    }

    if (HasFailure()) break;
  }

  // Tests with two inodes.
  for (int i = 0; i < 10000; i++) {
    LOG(INFO) << "=== Iteration " << i << " ===";
    int which = random() % 2;
    InodeHandle* h = which ? h1 : h2;
    InodeHandle* hp = which ? h2 : h1;
    size_t length = random() % (kBlockSize * 300);
    LOG(INFO) << "=== Setting length to " << length << " ===";

    uint64_t total = fs->ComputeTotalBlockUsage(length) + fs->ComputeTotalBlockUsage(hp->Length());
    ErrorCode expected = total > (256 - 5) ? ErrorCode::kDataBlocksExhausted : ErrorCode::kOk;
    ASSERT_EQ(h->SetLength(length), expected);

    ASSERT_EQ(fs->TEST_NumDataBlocksUsed(),
              fs->ComputeTotalBlockUsage(h1->Length()) + fs->ComputeTotalBlockUsage(h2->Length()));

    std::map<block_number_t, int> blocks1, blocks2;
    CollectBlocks(fs, h1, blocks1);
    CollectBlocks(fs, h2, blocks2);

    for (block_number_t b = 5; b < 256; b++) {
      bool is_allocated = fs->TEST_IsBlockAllocated(b);
      bool is_found = blocks1.count(b) > 0 || blocks2.count(b) > 0;
      ASSERT_EQ(is_allocated, is_found) << "(block " << b << ")";
    }

    if (HasFailure()) break;
  }

  fs->Unmount();
  delete fs;
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
