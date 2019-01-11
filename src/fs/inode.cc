#include "inode.h"
#include "assertions.h"

// FIXME: Need some kind of logging facility that works in the kernel and in tests and in userspace.
// Need test infrastructure for doing test-only callbacks/logging that can be tested against.
// Should fix BlockHandle so that it's RAII. Needing to clean up manually is silly.
// Need a way to test that I've released all blocks.
// Need a way to test that I called SetDirty on all blocks that have changed.
// Need randomized testing framework.
// Need to parametrize on block size and also eliminate lots of stuff from inode layout. This way I can test with much smaller blocks. Will need to allow superblock to span multiple blocks though.
// Fuzz testing with AFL.

#include <stdio.h>

static uint64_t RoundUp(uint64_t numerator, uint64_t denominator) {
  return (numerator + (denominator - 1)) / denominator;
}

static uint64_t NumBlocks(uint64_t bytes) {
  return RoundUp(bytes, kBlockSize);
}

struct FileSystem::SuperBlockLayout {
  // Total number of blocks on the disk.
  uint64_t num_blocks;

  block_number_t data_bitmap_start;
  block_number_t data_bitmap_end;  // One past the last block.

  block_number_t inode_bitmap_start;
  block_number_t inode_bitmap_end;  // One past the last block.

  // The inode table does not allow inodes to span across blocks.
  block_number_t inode_table_start;
  block_number_t inode_table_end;

  block_number_t data_blocks_start;
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
    inode_(inode),
    inode_block_(inode_block),
    index_(inode_index),
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

static const int kSubtreesPerBlock = kBlockSize / sizeof(block_number_t);

ErrorCode InodeHandle::AllocateDataBlocks(uint64_t data_block_index,
                                          block_number_t* subtrees,
                                          int subtree_count,
                                          uint64_t subtree_size,
                                          uint64_t cur_blocks,
                                          uint64_t new_blocks) {
  for (int i = 0; i < subtree_count; i++) {
    if (data_block_index >= new_blocks) {
      break;
    } else if (data_block_index >= cur_blocks) {
      BlockHandle* handle;
      ErrorCode err = fs_->AllocateDataBlock(&handle);
      if (err != ErrorCode::kOk) {
        return err;
      }
      subtrees[i] = handle->block_number();

      if (subtree_size > 1) {
        block_number_t* new_subtrees = reinterpret_cast<block_number_t*>(handle->Get());
        ErrorCode err = AllocateDataBlocks(data_block_index,
                                           new_subtrees,
                                           kSubtreesPerBlock,
                                           subtree_size / kSubtreesPerBlock,
                                           cur_blocks,
                                           new_blocks);

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

  return ErrorCode::kOk;
}

ErrorCode InodeHandle::FreeDataBlocks(uint64_t data_block_index,
                                      block_number_t* subtrees,
                                      int subtree_count,
                                      uint64_t subtree_size,
                                      uint64_t cur_blocks,
                                      uint64_t new_blocks) {
  if (data_block_index == 0) {
    // If we're able to store all pointers at this level in the inode, but not the
    // next level, then store them in the inode.
    uint64_t lower_subtree_size = subtree_size / kSubtreesPerBlock;
    if (subtree_size * InodeLayout::kNumTrees <= new_blocks &&
        lower_subtree_size * InodeLayout::kNumTrees > new_blocks) {
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
      if (subtree_size == 1) {
        fs_->FreeDataBlock(subtrees[i]);
      } else {
        BlockHandle* handle;
        ErrorCode err = env_->FetchBlock(subtrees[i], &handle);
        if (err != ErrorCode::kOk) {
          return err;
        }
        block_number_t* new_subtrees = reinterpret_cast<block_number_t*>(handle->Get());

        err = FreeDataBlocks(data_block_index,
                             new_subtrees,
                             kSubtreesPerBlock,
                             subtree_size / kSubtreesPerBlock,
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

uint64_t InodeHandle::ComputeTreeSize(uint64_t len) {
  uint64_t num_blocks = NumBlocks(len);
  uint64_t all_trees_span = InodeLayout::kNumTrees;

  while (all_trees_span < num_blocks) {
    all_trees_span *= kSubtreesPerBlock;
  }

  return all_trees_span / InodeLayout::kNumTrees;
}

ErrorCode InodeHandle::SetLength(uint64_t len) {
  uint64_t cur_blocks = NumBlocks(layout_->length);
  uint64_t new_blocks = NumBlocks(len);
  uint64_t cur_tree_size = ComputeTreeSize(layout_->length);

  layout_->length = len;
  inode_block_->SetDirty();

  if (new_blocks > cur_blocks) {
    printf("Will increase #blocks to %llu\n", new_blocks);

    ErrorCode err = AllocateDataBlocks(0, layout_->trees, InodeLayout::kNumTrees, cur_tree_size, cur_blocks, new_blocks);
    if (err != ErrorCode::kOk) {
      return err;
    }

    block_number_t prev_metablock = 0;
    uint64_t new_tree_size = ComputeTreeSize(len);
    while (cur_tree_size < new_tree_size) {
      BlockHandle* handle;
      err = fs_->AllocateDataBlock(&handle);
      if (err != ErrorCode::kOk) {
        // FIXME: Need to release the other blocks!
        return err;
      }

      block_number_t* subtrees = reinterpret_cast<block_number_t*>(handle->Get());
      if (prev_metablock) {
        subtrees[0] = prev_metablock;
      } else {
        memcpy(subtrees, layout_->trees, InodeLayout::kNumTrees * sizeof(block_number_t));
      }

      err = AllocateDataBlocks(0, subtrees, kSubtreesPerBlock, cur_tree_size, cur_blocks, new_blocks);
      if (err != ErrorCode::kOk) {
        // FIXME
        return err;
      }

      prev_metablock = handle->block_number();

      handle->SetDirty();
      handle->Release();

      cur_tree_size *= kSubtreesPerBlock;
    }

    if (prev_metablock) {
      layout_->trees[0] = prev_metablock;
      return AllocateDataBlocks(0, layout_->trees, InodeLayout::kNumTrees, cur_tree_size, cur_blocks, new_blocks);
    }
  } else if (new_blocks < cur_blocks) {
    return FreeDataBlocks(0, layout_->trees, InodeLayout::kNumTrees, cur_tree_size, cur_blocks, new_blocks);
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
                                    kSubtreesPerBlock,
                                    subtree_size / kSubtreesPerBlock,
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
  if (data_block_index >= NumBlocks(layout_->length)) {
    return ErrorCode::kFileTooSmall;
  }

  uint64_t tree_size = ComputeTreeSize(layout_->length);
  return GetDataBlockRecursive(data_block_index, layout_->trees, InodeLayout::kNumTrees, tree_size, result);
}

// static
ErrorCode FileSystem::Mount(FileSystemEnv* env, FileSystem** result) {
  BlockHandle* superblock;
  ErrorCode err = env->FetchBlock(0, &superblock);
  if (err != ErrorCode::kOk) {
    return err;
  }

  SuperBlockLayout* layout = reinterpret_cast<SuperBlockLayout*>(superblock->Get());
  uint64_t num_data_bitmap_blocks = layout->data_bitmap_end - layout->data_bitmap_start;
  BlockHandle** data_bitmaps = new BlockHandle*[num_data_bitmap_blocks];

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
  }

  *result = new FileSystem(env, superblock, data_bitmaps);
  return ErrorCode::kOk;
}

// static
ErrorCode FileSystem::Create(FileSystemEnv* env, uint64_t num_blocks, uint64_t max_inodes, FileSystem** result) {
  uint64_t data_bitmap_blocks = RoundUp(num_blocks, kBlockSize * 8);
  uint64_t inode_bitmap_blocks = RoundUp(max_inodes, kBlockSize * 8);
  uint64_t inode_table_blocks = RoundUp(max_inodes * sizeof(InodeHandle::InodeLayout), kBlockSize);

  BlockHandle* superblock;
  ErrorCode err = env->FetchBlock(0, &superblock);
  if (err != ErrorCode::kOk) {
    return err;
  }

  SuperBlockLayout* layout = reinterpret_cast<SuperBlockLayout*>(superblock->Get());
  layout->num_blocks = num_blocks;
  layout->data_bitmap_start = 1;
  layout->data_bitmap_end = layout->data_bitmap_start + data_bitmap_blocks;
  layout->inode_bitmap_start = layout->data_bitmap_end;
  layout->inode_bitmap_end = layout->inode_bitmap_start + inode_bitmap_blocks;
  layout->inode_table_start = layout->inode_bitmap_end;
  layout->inode_table_end = layout->inode_table_start + inode_table_blocks;
  layout->data_blocks_start = layout->inode_table_end;

  superblock->SetDirty();

  for (uint64_t i = 0; i < inode_bitmap_blocks; i++) {
    BlockHandle* handle;
    err = env->FetchBlock(layout->inode_bitmap_start + i, &handle);
    if (err != ErrorCode::kOk) {
      superblock->Release();
      return err;
    }

    memset(handle->Get(), 0, kBlockSize);
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

    memset(data_bitmaps[i]->Get(), 0, kBlockSize);
  }

  *result = new FileSystem(env, superblock, data_bitmaps);
  return ErrorCode::kOk;
}

FileSystem::FileSystem(FileSystemEnv* env, BlockHandle* superblock, BlockHandle** data_bitmap_blocks)
  : env_(env),
    superblock_(superblock),
    superblock_layout_(reinterpret_cast<SuperBlockLayout*>(superblock_->Get())),
    data_bitmap_blocks_(data_bitmap_blocks)
{
}

void FileSystem::Unmount() {
  SuperBlockLayout* layout = superblock_layout_;
  uint64_t num_data_bitmap_blocks = layout->data_bitmap_end - layout->data_bitmap_start;

  for (uint64_t i = 0; i < num_data_bitmap_blocks; i++) {
    data_bitmap_blocks_[i]->Release();
  }

  superblock_->Release();
}

void FileSystem::FindInode(inode_number_t inode, block_number_t* block, int* index) {
  const int inodes_per_block = kBlockSize / sizeof(InodeHandle::InodeLayout);
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
       bitmap_blocknum < superblock_layout_->inode_bitmap_end;
       bitmap_blocknum++) {
    BlockHandle* inode_bitmap_block;
    ErrorCode err = env_->FetchBlock(bitmap_blocknum, &inode_bitmap_block);
    if (err != ErrorCode::kOk) {
      return err;
    }

    unsigned char* bits = reinterpret_cast<unsigned char*>(inode_bitmap_block->Get());
    for (int byte = 0; byte < kBlockSize; byte++) {
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
  uint64_t num_data_bitmap_blocks = superblock_layout_->data_bitmap_end - superblock_layout_->data_bitmap_start;
  for (block_number_t i = 0; i < num_data_bitmap_blocks; i++) {
    unsigned char* bits = reinterpret_cast<unsigned char*>(data_bitmap_blocks_[i]->Get());
    for (int byte = 0; byte < kBlockSize; byte++) {
      for (int bit = 0; bit < 8; bit++) {
        if (bits[byte] & (1 << bit)) {
          block++;
        } else {
          // We found a free block!
          bits[byte] |= (1 << bit);
          data_bitmap_blocks_[i]->SetDirty();

          return env_->FetchBlock(block, result);
        }
      }
    }
  }

  return ErrorCode::kDataBlocksExhausted;
}

void FileSystem::FreeDataBlock(block_number_t block) {
  assert_ge(block, superblock_layout_->data_blocks_start);
  block -= superblock_layout_->data_blocks_start;

  uint64_t byte = block / 8;
  int bit = block % 8;

  uint64_t bitmap_block_number = byte / kBlockSize;
  int bitmap_block_index = byte % kBlockSize;

  BlockHandle* handle = data_bitmap_blocks_[bitmap_block_number];
  unsigned char* bits = reinterpret_cast<unsigned char*>(handle->Get());

  bits[bitmap_block_index] &= ~(1 << bit);

  handle->SetDirty();

  env_->FreeBlock(block);
}
