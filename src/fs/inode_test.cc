#include "inode.h"

#include "assertions.h"

#include "gtest/gtest.h"

class TestBlockHandle : public BlockHandle {
public:
  TestBlockHandle(block_number_t number, char* data)
    : number_(number), data_(data) {
    printf("Fetch block %llu\n", (unsigned long long)number_);
  }

  block_number_t block_number() const override { return number_; }
  char* Get() override {
    printf("Get block %llu\n", (unsigned long long)number_);
    return data_;
  }
  void SetDirty() override {
    printf("Dirty block %llu\n", (unsigned long long)number_);
  }
  void SyncNow() override {}
  void Release() override {
    printf("Release block %llu\n", (unsigned long long)number_);
    delete this;
  }

private:
  block_number_t number_;
  char* data_;
};

class TestEnv : public FileSystemEnv {
public:
  ErrorCode FetchBlock(block_number_t block, BlockHandle** result) override MUST_USE {
    assert_lt(block, 32);
    *result = new TestBlockHandle(block, blocks[block]);
    return ErrorCode::kOk;
  }

  ErrorCode FreeBlock(block_number_t block) override {
    return ErrorCode::kOk;
  }

  uint64_t Now() override { return 10; }

private:
  char blocks[32][kBlockSize];
};

class InodeTest : public testing::Test {
protected:
  TestEnv env_;
};

TEST_F(InodeTest, Basic) {
  FileSystem* fs;
  ASSERT_EQ(FileSystem::Create(&env_, 16, 32, &fs), ErrorCode::kOk);

  InodeHandle* h1;
  ASSERT_EQ(fs->AllocateInode(&h1), ErrorCode::kOk);

  ASSERT_EQ(h1->Length(), 0);
  block_number_t num;
  ASSERT_EQ(h1->GetDataBlock(0, &num), ErrorCode::kFileTooSmall);

  ASSERT_EQ(h1->SetLength(10), ErrorCode::kOk);
  ASSERT_EQ(h1->GetDataBlock(0, &num), ErrorCode::kOk);
  ASSERT_EQ(num, 4);

  InodeHandle* h2;
  ASSERT_EQ(fs->AllocateInode(&h2), ErrorCode::kOk);
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
