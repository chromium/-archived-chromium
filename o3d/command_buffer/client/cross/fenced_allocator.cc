/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


// This file contains the implementation of the FencedAllocator class.

#include "command_buffer/client/cross/fenced_allocator.h"
#include <algorithm>
#include "command_buffer/client/cross/cmd_buffer_helper.h"

namespace o3d {
namespace command_buffer {

#ifndef COMPILER_MSVC
const FencedAllocator::Offset FencedAllocator::kInvalidOffset;
#endif

FencedAllocator::~FencedAllocator() {
  // Free blocks pending tokens.
  for (unsigned int i = 0; i < blocks_.size(); ++i) {
    if (blocks_[i].state == FREE_PENDING_TOKEN) {
      i = WaitForTokenAndFreeBlock(i);
    }
  }
  DCHECK_EQ(blocks_.size(), 1);
  DCHECK_EQ(blocks_[0].state, FREE);
}

// Looks for a non-allocated block that is big enough. Search in the FREE
// blocks first (for direct usage), first-fit, then in the FREE_PENDING_TOKEN
// blocks, waiting for them. The current implementation isn't smart about
// optimizing what to wait for, just looks inside the block in order (first-fit
// as well).
FencedAllocator::Offset FencedAllocator::Alloc(unsigned int size) {
  // Similarly to malloc, an allocation of 0 allocates at least 1 byte, to
  // return different pointers every time.
  if (size == 0) size = 1;

  // Try first to allocate in a free block.
  for (unsigned int i = 0; i < blocks_.size(); ++i) {
    Block &block = blocks_[i];
    if (block.state == FREE && block.size >= size) {
      return AllocInBlock(i, size);
    }
  }

  // No free block is available. Look for blocks pending tokens, and wait for
  // them to be re-usable.
  for (unsigned int i = 0; i < blocks_.size(); ++i) {
    if (blocks_[i].state != FREE_PENDING_TOKEN)
      continue;
    i = WaitForTokenAndFreeBlock(i);
    if (blocks_[i].size >= size)
      return AllocInBlock(i, size);
  }
  return kInvalidOffset;
}

// Looks for the corresponding block, mark it FREE, and collapse it if
// necessary.
void FencedAllocator::Free(FencedAllocator::Offset offset) {
  BlockIndex index = GetBlockByOffset(offset);
  DCHECK_NE(blocks_[index].state, FREE);
  blocks_[index].state = FREE;
  CollapseFreeBlock(index);
}

// Looks for the corresponding block, mark it FREE_PENDING_TOKEN.
void FencedAllocator::FreePendingToken(FencedAllocator::Offset offset,
                                       unsigned int token) {
  BlockIndex index = GetBlockByOffset(offset);
  Block &block = blocks_[index];
  block.state = FREE_PENDING_TOKEN;
  block.token = token;
}

// Gets the max of the size of the blocks marked as free.
unsigned int FencedAllocator::GetLargestFreeSize() {
  unsigned int max_size = 0;
  for (unsigned int i = 0; i < blocks_.size(); ++i) {
    Block &block = blocks_[i];
    if (block.state == FREE)
      max_size = std::max(max_size, block.size);
  }
  return max_size;
}

// Gets the size of the largest segment of blocks that are either FREE or
// FREE_PENDING_TOKEN.
unsigned int FencedAllocator::GetLargestFreeOrPendingSize() {
  unsigned int max_size = 0;
  unsigned int current_size = 0;
  for (unsigned int i = 0; i < blocks_.size(); ++i) {
    Block &block = blocks_[i];
    if (block.state == IN_USE) {
      max_size = std::max(max_size, current_size);
      current_size = 0;
    } else {
      DCHECK(block.state == FREE || block.state == FREE_PENDING_TOKEN);
      current_size += block.size;
    }
  }
  return std::max(max_size, current_size);
}

// Makes sure that:
// - there is at least one block.
// - there are no contiguous FREE blocks (they should have been collapsed).
// - the successive offsets match the block sizes, and they are in order.
bool FencedAllocator::CheckConsistency() {
  if (blocks_.size() < 1) return false;
  for (unsigned int i = 0; i < blocks_.size() - 1; ++i) {
    Block &current = blocks_[i];
    Block &next = blocks_[i + 1];
    // This test is NOT included in the next one, because offset is unsigned.
    if (next.offset <= current.offset)
      return false;
    if (next.offset != current.offset + current.size)
      return false;
    if (current.state == FREE && next.state == FREE)
      return false;
  }
  return true;
}

// Collapse the block to the next one, then to the previous one. Provided the
// structure is consistent, those are the only blocks eligible for collapse.
FencedAllocator::BlockIndex FencedAllocator::CollapseFreeBlock(
    BlockIndex index) {
  if (index + 1 < blocks_.size()) {
    Block &next = blocks_[index + 1];
    if (next.state == FREE) {
      blocks_[index].size += next.size;
      blocks_.erase(blocks_.begin() + index + 1);
    }
  }
  if (index > 0) {
    Block &prev = blocks_[index - 1];
    if (prev.state == FREE) {
      prev.size += blocks_[index].size;
      blocks_.erase(blocks_.begin() + index);
      --index;
    }
  }
  return index;
}

// Waits for the block's token, then mark the block as free, then collapse it.
FencedAllocator::BlockIndex FencedAllocator::WaitForTokenAndFreeBlock(
    BlockIndex index) {
  Block &block = blocks_[index];
  DCHECK_EQ(block.state, FREE_PENDING_TOKEN);
  helper_->WaitForToken(block.token);
  block.state = FREE;
  return CollapseFreeBlock(index);
}

// If the block is exactly the requested size, simply mark it IN_USE, otherwise
// split it and mark the first one (of the requested size) IN_USE.
FencedAllocator::Offset FencedAllocator::AllocInBlock(BlockIndex index,
                                                      unsigned int size) {
  Block &block = blocks_[index];
  DCHECK_GE(block.size, size);
  DCHECK_EQ(block.state, FREE);
  Offset offset = block.offset;
  if (block.size == size) {
    block.state = IN_USE;
    return offset;
  }
  Block newblock = { FREE, offset + size, block.size - size, kUnusedToken};
  block.state = IN_USE;
  block.size = size;
  // this is the last thing being done because it may invalidate block;
  blocks_.insert(blocks_.begin() + index + 1, newblock);
  return offset;
}

// The blocks are in offset order, so we can do a binary search.
FencedAllocator::BlockIndex FencedAllocator::GetBlockByOffset(Offset offset) {
  Block templ = { IN_USE, offset, 0, kUnusedToken };
  Container::iterator it = std::lower_bound(blocks_.begin(), blocks_.end(),
                                            templ, OffsetCmp());
  DCHECK(it != blocks_.end() && it->offset == offset);
  return it-blocks_.begin();
}

}  // namespace command_buffer
}  // namespace o3d
