#include "inode.h"
#include "base/assertions.h"
#include "base/output_stream.h"
#include "base/testing.h"

// FIXME:
// Main thing is to test error cases, like running out of blocks or disk errors.
// Should fix BlockHandle so that it's RAII. Needing to clean up manually is silly.
// Need a way to test that I've released all blocks.
// Need a way to test that I called SetDirty on all blocks that have changed.

#include <stdio.h>

static uint64_t RoundUp(uint64_t numerator, uint64_t denominator) {
  return (numerator + (denominator - 1)) / denominator;
}

struct FileSystem::SuperBlockLayout {
  uint64_t block_size;
  uint64_t num_blocks;  // Total number of blocks on the disk.

  block_number_t data_bitmap_start;
  block_number_t data_bitmap_limit;  // One past the last block.

  block_number_t inode_bitmap_start;
  block_number_t inode_bitmap_limit;  // One past the last block.

  // The inode table does not allow inodes to span across blocks.
  block_number_t inode_table_start;
  block_number_t inode_table_limit;

  block_number_t data_blocks_start;
  uint64_t data_blocks_used;
} __attribute__((packed));

struct InodeHandle::InodeLayout {
  uint16_t refcount;
  uint16_t mode;
  uint32_t create_time;
  uint32_t modify_time;
  uint32_t uid;
  uint32_t gid;
  uint32_t flags;

  static const int kNumTrees = 12;

  uint64_t length;
  block_number_t trees[kNumTrees];
} __attribute__((packed));

InodeHandle::InodeHandle(FileSystem* fs,
                         inode_number_t inode,
                         BlockHandle* inode_block,
                         int inode_index,
                         InodeHandle::InodeLayout* layout)
  : fs_(fs),
    env_(fs->env()),
    inode_block_(inode_block),
    layout_(layout)
{
}

InodeHandle::~InodeHandle() {
  inode_block_->Release();
}

void InodeHandle::IncRef() {
  layout_->refcount++;
  inode_block_->SetDirty();
}

void InodeHandle::DecRef() {
  uint16_t rc = --layout_->refcount;
  inode_block_->SetDirty();
  if (rc == 0) {
    delete this;
  }
}

uint32_t InodeHandle::CreateTime() {
  return layout_->create_time;
}

void InodeHandle::SetCreateTime(uint32_t t) {
  layout_->create_time = t;
  inode_block_->SetDirty();
}

uint32_t InodeHandle::ModifyTime() {
  return layout_->modify_time;
}

void InodeHandle::SetModifyTime(uint32_t t) {
  layout_->modify_time = t;
  inode_block_->SetDirty();
}

uint16_t InodeHandle::Mode() {
  return layout_->mode;
}

void InodeHandle::SetMode(uint16_t mode) {
  layout_->mode = mode;
  inode_block_->SetDirty();
}

uint32_t InodeHandle::User() {
  return layout_->uid;
}

void InodeHandle::SetUser(uint32_t user) {
  layout_->uid = user;
  inode_block_->SetDirty();
}

uint32_t InodeHandle::Group() {
  return layout_->gid;
}

void InodeHandle::SetGroup(uint32_t group) {
  layout_->gid = group;
  inode_block_->SetDirty();
}

uint32_t InodeHandle::Flags() {
  return layout_->flags;
}

void InodeHandle::SetFlags(uint32_t flags) {
  layout_->flags = flags;
  inode_block_->SetDirty();
}

uint64_t InodeHandle::Length() {
  return layout_->length;
}

uint64_t InodeHandle::SubtreesPerBlock() {
  return fs_->SubtreesPerBlock();
}

ErrorCode InodeHandle::AllocateDataBlocks(uint64_t data_block_index,
                                          block_number_t* subtrees,
                                          int subtree_count,
                                          uint64_t subtree_size,
                                          uint64_t cur_blocks,
                                          uint64_t new_blocks,
                                          uint64_t* end_block) {
  LOG(DEBUG) << "ADB " << data_block_index << " " << subtree_size;
  for (int i = 0; i < subtree_count; i++) {
    if (data_block_index >= new_blocks) {
      break;
    }

    if (data_block_index + subtree_size > cur_blocks) {
      BlockHandle* handle;
      if (data_block_index >= cur_blocks) {
        ErrorCode err = fs_->AllocateDataBlock(&handle);
        if (subtree_size == 1) {
          LOG(DEBUG) << "  (is data block for file)";
        } else {
          LOG(DEBUG) << "  (is metadata block)";
        }
        if (err != ErrorCode::kOk) {
          return err;
        }
        subtrees[i] = handle->block_number();
      } else {
        ErrorCode err = env_->FetchBlock(subtrees[i], &handle);
        if (err != ErrorCode::kOk) {
          return err;
        }
      }

      if (subtree_size > 1) {
        block_number_t* new_subtrees = reinterpret_cast<block_number_t*>(handle->Get());
        ErrorCode err = AllocateDataBlocks(data_block_index,
                                           new_subtrees,
                                           SubtreesPerBlock(),
                                           subtree_size / SubtreesPerBlock(),
                                           cur_blocks,
                                           new_blocks,
                                           end_block);

        if (err != ErrorCode::kOk) {
          handle->Release();
          return err;
        }

        handle->SetDirty();
        handle->Release();
      }
    }
    data_block_index += subtree_size;
  }

  *end_block = data_block_index;

  return ErrorCode::kOk;
}

ErrorCode InodeHandle::FreeDataBlocks(uint64_t data_block_index,
                                      block_number_t* subtrees,
                                      int subtree_count,
                                      uint64_t subtree_size,
                                      uint64_t cur_blocks,
                                      uint64_t new_blocks) {
  LOG(DEBUG) << "FDB " << data_block_index << " " << subtree_size;

  uint64_t lower_subtree_size = subtree_size / SubtreesPerBlock();

  if (data_block_index == 0) {
    // If we're able to store all pointers at this level in the inode, but not the
    // next level, then store them in the inode.
    if (subtree_size * InodeLayout::kNumTrees >= new_blocks &&
        lower_subtree_size * InodeLayout::kNumTrees < new_blocks) {
      bool change = false;
      for (int i = 0; i * subtree_size < new_blocks; i++) {
        if (layout_->trees[i] != subtrees[i]) {
          change = true;
          layout_->trees[i] = subtrees[i];
        }
      }
      if (change) {
        inode_block_->SetDirty();
      }
    }
  }

  for (int i = 0; i < subtree_count; i++) {
    if (data_block_index >= cur_blocks) {
      break;
    } else if (data_block_index + subtree_size > new_blocks) {
      if (data_block_index >= new_blocks || (data_block_index == 0 && lower_subtree_size * InodeLayout::kNumTrees >= new_blocks)) {
        fs_->FreeDataBlock(subtrees[i]);
      }

      if (subtree_size > 1) {
        BlockHandle* handle;
        ErrorCode err = env_->FetchBlock(subtrees[i], &handle);
        if (err != ErrorCode::kOk) {
          return err;
        }
        block_number_t* new_subtrees = reinterpret_cast<block_number_t*>(handle->Get());

        err = FreeDataBlocks(data_block_index,
                             new_subtrees,
                             SubtreesPerBlock(),
                             subtree_size / SubtreesPerBlock(),
                             cur_blocks,
                             new_blocks);

        handle->Release();
        if (err != ErrorCode::kOk) {
          return err;
        }
      }
    }
    data_block_index += subtree_size;
  }

  return ErrorCode::kOk;
}

uint64_t InodeHandle::TEST_GetTreeSizeInBlocks() {
  return fs_->ComputeTreeSize(layout_->length);
}

ErrorCode InodeHandle::SetLength(uint64_t len) {
  if (len > layout_->length) {
    uint64_t diff = fs_->ComputeTotalBlockUsage(len) - fs_->ComputeTotalBlockUsage(layout_->length);
    LOG(INFO) << "diff=" << diff;
    FileSystem::SuperBlockLayout* super = fs_->superblock_layout_;
    uint64_t available = super->num_blocks - super->data_blocks_start - super->data_blocks_used;
    LOG(INFO) << "avail=" << available;
    if (diff > available) {
      return ErrorCode::kDataBlocksExhausted;
    }
  }

  uint64_t cur_blocks = RoundUp(layout_->length, fs_->block_size());
  uint64_t new_blocks = RoundUp(len, fs_->block_size());
  uint64_t cur_tree_size = fs_->ComputeTreeSize(layout_->length);

  LOG(DEBUG) << "new_blocks " << new_blocks;

  layout_->length = len;
  inode_block_->SetDirty();

  if (new_blocks > cur_blocks) {
    ErrorCode err = AllocateDataBlocks(0, layout_->trees, InodeLayout::kNumTrees, cur_tree_size, cur_blocks, new_blocks, &cur_blocks);
    if (err != ErrorCode::kOk) {
      return err;
    }

    LOG(DEBUG) << "Finished first allocations";

    block_number_t prev_metablock = 0;
    uint64_t new_tree_size = fs_->ComputeTreeSize(len);
    while (cur_tree_size < new_tree_size) {
      LOG(DEBUG) << "cur_tree_size = " << cur_tree_size;
      LOG(DEBUG) << "cur_blocks = " << cur_blocks;

      BlockHandle* handle;
      err = fs_->AllocateDataBlock(&handle);
      if (err != ErrorCode::kOk) {
        // FIXME: Need to release the other blocks!
        return err;
      }
      LOG(DEBUG) << "  (is metadata block)";

      block_number_t* subtrees = reinterpret_cast<block_number_t*>(handle->Get());
      if (prev_metablock) {
        subtrees[0] = prev_metablock;
      } else {
        memcpy(subtrees, layout_->trees, InodeLayout::kNumTrees * sizeof(block_number_t));
      }

      err = AllocateDataBlocks(0, subtrees, SubtreesPerBlock(), cur_tree_size, cur_blocks, new_blocks, &cur_blocks);
      if (err != ErrorCode::kOk) {
        // FIXME
        return err;
      }

      prev_metablock = handle->block_number();

      handle->SetDirty();
      handle->Release();

      cur_tree_size *= SubtreesPerBlock();
    }

    if (prev_metablock) {
      layout_->trees[0] = prev_metablock;
      return AllocateDataBlocks(0, layout_->trees, InodeLayout::kNumTrees, cur_tree_size, cur_blocks, new_blocks, &cur_blocks);
    }
  } else if (new_blocks < cur_blocks) {
    // Use a copy of `trees`, since we update it inside the function.
    block_number_t trees[InodeLayout::kNumTrees];
    memcpy(trees, layout_->trees, sizeof(block_number_t) * InodeLayout::kNumTrees);

    return FreeDataBlocks(0, trees, InodeLayout::kNumTrees, cur_tree_size, cur_blocks, new_blocks);
  }

  return ErrorCode::kOk;
}

ErrorCode InodeHandle::GetDataBlockRecursive(uint64_t data_block_index,
                                             block_number_t* subtrees,
                                             int subtree_count,
                                             uint64_t subtree_size,
                                             block_number_t* result) {
  for (int i = 0; i < subtree_count; i++) {
    if (data_block_index < subtree_size) {
      if (subtree_size == 1) {
        *result = subtrees[i];
        return ErrorCode::kOk;
      } else {
        BlockHandle* handle;
        ErrorCode err = env_->FetchBlock(subtrees[i], &handle);
        if (err != ErrorCode::kOk) {
          return err;
        }
        block_number_t* new_subtrees = reinterpret_cast<block_number_t*>(handle->Get());

        err = GetDataBlockRecursive(data_block_index,
                                    new_subtrees,
                                    SubtreesPerBlock(),
                                    subtree_size / SubtreesPerBlock(),
                                    result);

        handle->Release();
        return err;
      }
    }

    data_block_index -= subtree_size;
  }

  panic("Ran out of blocks!");
}

ErrorCode InodeHandle::GetDataBlock(uint64_t data_block_index, block_number_t* result) {
  if (data_block_index >= RoundUp(layout_->length, fs_->block_size())) {
    return ErrorCode::kFileTooSmall;
  }

  uint64_t tree_size = fs_->ComputeTreeSize(layout_->length);
  return GetDataBlockRecursive(data_block_index, layout_->trees, InodeLayout::kNumTrees, tree_size, result);
}

block_number_t* InodeHandle::TEST_GetTrees() {
  return layout_->trees;
}

uint64_t FileSystem::ComputeTreeSize(uint64_t len) {
  uint64_t num_blocks = RoundUp(len, block_size());
  uint64_t all_trees_span = InodeHandle::InodeLayout::kNumTrees;

  while (all_trees_span < num_blocks) {
    all_trees_span *= SubtreesPerBlock();
  }

  return all_trees_span / InodeHandle::InodeLayout::kNumTrees;
}

uint64_t FileSystem::ComputeTotalBlockUsage(uint64_t len) {
  uint64_t num_blocks = RoundUp(len, block_size());
  uint64_t total_blocks = 0;
  uint64_t tree_size = ComputeTreeSize(len);

  while (tree_size > 0) {
    total_blocks += RoundUp(num_blocks, tree_size);
    tree_size /= SubtreesPerBlock();
  }

  return total_blocks;
}

uint64_t FileSystem::SubtreesPerBlock() const {
  return block_size() / sizeof(block_number_t);
}

// static
ErrorCode FileSystem::Mount(FileSystemEnv* env, FileSystem** result) {
  BlockHandle* superblock;
  ErrorCode err = env->FetchBlock(0, &superblock);
  if (err != ErrorCode::kOk) {
    return err;
  }

  // FIXME: Should check validity of some things like block size, etc.

  SuperBlockLayout* layout = reinterpret_cast<SuperBlockLayout*>(superblock->Get());
  uint64_t num_data_bitmap_blocks = layout->data_bitmap_limit - layout->data_bitmap_start;
  BlockHandle** data_bitmaps = new BlockHandle*[num_data_bitmap_blocks];

  uint64_t data_blocks_used = 0;
  for (uint64_t i = 0; i < num_data_bitmap_blocks; i++) {
    err = env->FetchBlock(layout->data_bitmap_start + i, &data_bitmaps[i]);
    if (err != ErrorCode::kOk) {
      superblock->Release();
      for (uint64_t j = 0; j < i; j++) {
        data_bitmaps[j]->Release();
      }
      delete[] data_bitmaps;
      return err;
    }

    unsigned char* bits = reinterpret_cast<unsigned char*>(data_bitmaps[i]->Get());
    for (int j = 0; j < int(layout->block_size); j++) {
      for (int k = 0; k < 8; k++) {
        if (bits[j] & (1 << k)) {
          data_blocks_used++;
        }
      }
    }
  }

  layout->data_blocks_used = data_blocks_used;

  *result = new FileSystem(env, layout->block_size, superblock, data_bitmaps);
  return ErrorCode::kOk;
}

// static
ErrorCode FileSystem::Create(FileSystemEnv* env,
                             uint64_t block_size,
                             uint64_t num_blocks,
                             uint64_t max_inodes,
                             FileSystem** result) {
  assert_ge(block_size, sizeof(SuperBlockLayout));
  assert_ge(block_size, sizeof(InodeHandle::InodeLayout));

  uint64_t data_bitmap_blocks = RoundUp(num_blocks, block_size * 8);
  uint64_t inode_bitmap_blocks = RoundUp(max_inodes, block_size * 8);
  uint64_t inode_table_blocks = RoundUp(max_inodes * sizeof(InodeHandle::InodeLayout), block_size);

  BlockHandle* superblock;
  ErrorCode err = env->FetchBlock(0, &superblock);
  if (err != ErrorCode::kOk) {
    return err;
  }

  SuperBlockLayout* layout = reinterpret_cast<SuperBlockLayout*>(superblock->Get());
  layout->block_size = block_size;
  layout->num_blocks = num_blocks;
  layout->data_bitmap_start = 1;
  layout->data_bitmap_limit = layout->data_bitmap_start + data_bitmap_blocks;
  layout->inode_bitmap_start = layout->data_bitmap_limit;
  layout->inode_bitmap_limit = layout->inode_bitmap_start + inode_bitmap_blocks;
  layout->inode_table_start = layout->inode_bitmap_limit;
  layout->inode_table_limit = layout->inode_table_start + inode_table_blocks;
  layout->data_blocks_start = layout->inode_table_limit;
  layout->data_blocks_used = 0;

  superblock->SetDirty();

  for (uint64_t i = 0; i < inode_bitmap_blocks; i++) {
    BlockHandle* handle;
    err = env->FetchBlock(layout->inode_bitmap_start + i, &handle);
    if (err != ErrorCode::kOk) {
      superblock->Release();
      return err;
    }

    memset(handle->Get(), 0, block_size);
    handle->Release();
  }

  BlockHandle** data_bitmaps = new BlockHandle*[data_bitmap_blocks];
  for (uint64_t i = 0; i < data_bitmap_blocks; i++) {
    err = env->FetchBlock(layout->data_bitmap_start + i, &data_bitmaps[i]);
    if (err != ErrorCode::kOk) {
      superblock->Release();
      for (uint64_t j = 0; j < i; j++) {
        data_bitmaps[j]->Release();
      }
      delete[] data_bitmaps;
      return err;
    }

    memset(data_bitmaps[i]->Get(), 0, block_size);
  }

  *result = new FileSystem(env, block_size, superblock, data_bitmaps);
  return ErrorCode::kOk;
}

FileSystem::FileSystem(FileSystemEnv* env, uint64_t block_size, BlockHandle* superblock, BlockHandle** data_bitmap_blocks)
  : env_(env),
    block_size_(block_size),
    superblock_(superblock),
    superblock_layout_(reinterpret_cast<SuperBlockLayout*>(superblock_->Get())),
    data_bitmap_blocks_(data_bitmap_blocks)
{
}

void FileSystem::Unmount() {
  SuperBlockLayout* layout = superblock_layout_;
  uint64_t num_data_bitmap_blocks = layout->data_bitmap_limit - layout->data_bitmap_start;

  for (uint64_t i = 0; i < num_data_bitmap_blocks; i++) {
    data_bitmap_blocks_[i]->Release();
  }

  superblock_->SetDirty();
  superblock_->Release();
}

void FileSystem::FindInode(inode_number_t inode, block_number_t* block, int* index) {
  const int inodes_per_block = block_size() / sizeof(InodeHandle::InodeLayout);
  *block = superblock_layout_->inode_table_start + (inode / inodes_per_block);
  *index = inode % inodes_per_block;
}

ErrorCode FileSystem::FetchInode(inode_number_t inode, InodeHandle** result) {
  int inode_index;
  block_number_t block_number;
  FindInode(inode, &block_number, &inode_index);

  BlockHandle* handle;
  ErrorCode err = env_->FetchBlock(block_number, &handle);
  if (err != ErrorCode::kOk) {
    return err;
  }

  InodeHandle::InodeLayout* layout = reinterpret_cast<InodeHandle::InodeLayout*>(handle->Get()) + inode_index;
  *result = new InodeHandle(this, inode, handle, inode_index, layout);
  return ErrorCode::kOk;
}

ErrorCode FileSystem::CreateInode(inode_number_t inode, InodeHandle** result) {
  int inode_index;
  block_number_t block_number;
  FindInode(inode, &block_number, &inode_index);

  BlockHandle* handle;
  ErrorCode err = env_->FetchBlock(block_number, &handle);
  if (err != ErrorCode::kOk) {
    return err;
  }

  InodeHandle::InodeLayout* layout = reinterpret_cast<InodeHandle::InodeLayout*>(handle->Get()) + inode_index;

  layout->refcount = 1;
  layout->create_time = layout->modify_time = env_->Now();
  layout->length = 0;

  for (int i = 0; i < InodeHandle::InodeLayout::kNumTrees; i++) {
    layout->trees[i] = 0;
  }

  handle->SetDirty();

  *result = new InodeHandle(this, inode, handle, inode_index, layout);
  return ErrorCode::kOk;
}

ErrorCode FileSystem::AllocateInode(InodeHandle** result) {
  inode_number_t inode = 0;
  for (block_number_t bitmap_blocknum = superblock_layout_->inode_bitmap_start;
       bitmap_blocknum < superblock_layout_->inode_bitmap_limit;
       bitmap_blocknum++) {
    BlockHandle* inode_bitmap_block;
    ErrorCode err = env_->FetchBlock(bitmap_blocknum, &inode_bitmap_block);
    if (err != ErrorCode::kOk) {
      return err;
    }

    unsigned char* bits = reinterpret_cast<unsigned char*>(inode_bitmap_block->Get());
    for (int byte = 0; byte < int(block_size()); byte++) {
      for (int bit = 0; bit < 8; bit++) {
        if (bits[byte] & (1 << bit)) {
          inode++;
        } else {
          // We found a free inode!
          bits[byte] |= (1 << bit);
          inode_bitmap_block->SetDirty();
          inode_bitmap_block->Release();

          return CreateInode(inode, result);
        }
      }
    }

    inode_bitmap_block->Release();
  }

  return ErrorCode::kInodesExhausted;
}

ErrorCode FileSystem::AllocateDataBlock(BlockHandle** result) {
  block_number_t block = superblock_layout_->data_blocks_start;
  uint64_t num_data_bitmap_blocks = superblock_layout_->data_bitmap_limit - superblock_layout_->data_bitmap_start;
  for (block_number_t i = 0; i < num_data_bitmap_blocks; i++) {
    unsigned char* bits = reinterpret_cast<unsigned char*>(data_bitmap_blocks_[i]->Get());
    for (int byte = 0; byte < int(block_size()); byte++) {
      for (int bit = 0; bit < 8; bit++) {
        if (bits[byte] & (1 << bit)) {
          block++;
        } else {
          superblock_layout_->data_blocks_used++;

          // We found a free block!
          bits[byte] |= (1 << bit);
          data_bitmap_blocks_[i]->SetDirty();

          TEST_ONLY(env_->Test_DataBlockAllocated(block));

          LOG(DEBUG) << "Data block allocated " << block;
          return env_->FetchBlock(block, result);
        }
      }
    }
  }

  return ErrorCode::kDataBlocksExhausted;
}

void FileSystem::FreeDataBlock(block_number_t block) {
  LOG(DEBUG) << "FreeBlock " << block;

  superblock_layout_->data_blocks_used--;

  assert_ge(block, superblock_layout_->data_blocks_start);
  assert_lt(block, superblock_layout_->num_blocks);
  block -= superblock_layout_->data_blocks_start;

  uint64_t byte = block / 8;
  int bit = block % 8;

  uint64_t bitmap_block_number = byte / block_size();
  int bitmap_block_index = byte % block_size();

  BlockHandle* handle = data_bitmap_blocks_[bitmap_block_number];
  unsigned char* bits = reinterpret_cast<unsigned char*>(handle->Get());

  bits[bitmap_block_index] &= ~(1 << bit);

  handle->SetDirty();

  TEST_ONLY(env_->Test_DataBlockDeallocated(block));

  env_->FreeBlock(block);
}

bool FileSystem::TEST_IsBlockAllocated(block_number_t block) {
  assert_ge(block, superblock_layout_->data_blocks_start);
  assert_lt(block, superblock_layout_->num_blocks);
  block -= superblock_layout_->data_blocks_start;

  uint64_t byte = block / 8;
  int bit = block % 8;

  uint64_t bitmap_block_number = byte / block_size();
  int bitmap_block_index = byte % block_size();

  BlockHandle* handle = data_bitmap_blocks_[bitmap_block_number];
  unsigned char* bits = reinterpret_cast<unsigned char*>(handle->Get());

  return bits[bitmap_block_index] & (1 << bit);
}

uint64_t FileSystem::TEST_NumDataBlocksUsed() {
  return superblock_layout_->data_blocks_used;
}
