#ifndef fs_inode_h
#define fs_inode_h

// FIXME: Should handle cases where memory allocator fails!
// What happens to InodeHandles when the file system is unmounted?
// Should I maintain some sort of refcount?

#include "base/types.h"

using block_number_t = uint64_t;
using inode_number_t = uint64_t;

static const int kBlockSize = 4096;

enum class ErrorCode {
  kOk,
  kReadError,
  kDataBlocksExhausted,
  kInodesExhausted,
  kFileTooSmall,
};

class BlockHandle {
public:
  virtual ~BlockHandle() {}

  virtual block_number_t block_number() const = 0;
  virtual char* Get() = 0;
  virtual void SetDirty() = 0;
  virtual void SyncNow() = 0;
  virtual void Release() = 0;
};

#define MUST_USE __attribute__((warn_unused_result))

class FileSystemEnv {
public:
  virtual ErrorCode FetchBlock(block_number_t block, BlockHandle** result) MUST_USE = 0;
  virtual ErrorCode FreeBlock(block_number_t block) = 0;

  virtual uint64_t Now() = 0;

  virtual void Test_DataBlockAllocated(block_number_t block) {}
  virtual void Test_DataBlockDeallocated(block_number_t block) {}
};

class FileSystem;

class InodeHandle {
public:
  void IncRef();
  void DecRef();

  // Metadata

  uint32_t CreateTime();
  void SetCreateTime(uint32_t t);

  uint32_t ModifyTime();
  void SetModifyTime(uint32_t t);

  uint16_t Mode();
  void SetMode(uint16_t mode);

  uint32_t User();
  void SetUser(uint32_t user);

  uint32_t Group();
  void SetGroup(uint32_t group);

  uint32_t Flags();
  void SetFlags(uint32_t flags);

  // Data

  uint64_t Length();
  ErrorCode SetLength(uint64_t len);

  ErrorCode GetDataBlock(uint64_t block_index, block_number_t* result) MUST_USE;

private:
  struct InodeLayout;

  friend class FileSystem;

  InodeHandle(FileSystem* fs,
              inode_number_t inode,
              BlockHandle* inode_block,
              int inode_index,
              InodeHandle::InodeLayout* layout);
  ~InodeHandle();

  uint64_t ComputeTreeSize(uint64_t len);

  ErrorCode AllocateDataBlocks(uint64_t data_block_index,
                               block_number_t* subtrees,
                               int subtree_count,
                               uint64_t subtree_size,
                               uint64_t cur_blocks,
                               uint64_t new_blocks);

  ErrorCode FreeDataBlocks(uint64_t data_block_index,
                           block_number_t* subtrees,
                           int subtree_count,
                           uint64_t subtree_size,
                           uint64_t cur_blocks,
                           uint64_t new_blocks);

  ErrorCode GetDataBlockRecursive(uint64_t data_block_index,
                                  block_number_t* subtrees,
                                  int subtree_count,
                                  uint64_t subtree_size,
                                  block_number_t* result);

  FileSystem* fs_;
  FileSystemEnv* env_;
  BlockHandle* inode_block_;
  InodeLayout* layout_;
};

class FileSystem {
public:
  static ErrorCode Mount(FileSystemEnv* env, FileSystem** result) MUST_USE;
  static ErrorCode Create(FileSystemEnv* env, uint64_t num_blocks, uint64_t max_inodes, FileSystem** result) MUST_USE;

  void Unmount();

  ErrorCode FetchInode(inode_number_t inode, InodeHandle** result) MUST_USE;
  ErrorCode AllocateInode(InodeHandle** result) MUST_USE;

  FileSystemEnv* env() const { return env_; }

private:
  struct SuperBlockLayout;

  friend class InodeHandle;
  ErrorCode AllocateDataBlock(BlockHandle** result);
  void FreeDataBlock(block_number_t block);

  ErrorCode CreateInode(inode_number_t inode, InodeHandle** result) MUST_USE;

  void FindInode(inode_number_t inode, block_number_t* block, int* index);

  FileSystem(FileSystemEnv* env, BlockHandle* superblock, BlockHandle** block_bitmap);

  FileSystemEnv* env_;
  BlockHandle* superblock_;
  SuperBlockLayout* superblock_layout_;

  // For simplicity, we keep all the block bitmap blocks in memory.
  BlockHandle** data_bitmap_blocks_;
};

#endif
