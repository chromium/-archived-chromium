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


// This file contains the definition of the FencedAllocator class.

#ifndef O3D_COMMAND_BUFFER_CLIENT_CROSS_FENCED_ALLOCATOR_H_
#define O3D_COMMAND_BUFFER_CLIENT_CROSS_FENCED_ALLOCATOR_H_

#include <vector>
#include "base/basictypes.h"
#include "command_buffer/common/cross/logging.h"

namespace o3d {
namespace command_buffer {
class CommandBufferHelper;

// FencedAllocator provides a mechanism to manage allocations within a fixed
// block of memory (storing the book-keeping externally). Furthermore this
// class allows to free data "pending" the passage of a command buffer token,
// that is, the memory won't be reused until the command buffer has processed
// that token.
//
// NOTE: Although this class is intended to be used in the command buffer
// environment which is multi-process, this class isn't "thread safe", because
// it isn't meant to be shared across modules. It is thread-compatible though
// (see http://www.corp.google.com/eng/doc/cpp_primer.html#thread_safety).
class FencedAllocator {
 public:
  typedef unsigned int Offset;
  // Invalid offset, returned by Alloc in case of failure.
  static const Offset kInvalidOffset = 0xffffffffU;

  // Creates a FencedAllocator. Note that the size of the buffer is passed, but
  // not its base address: everything is handled as offsets into the buffer.
  FencedAllocator(unsigned int size,
                  CommandBufferHelper *helper)
      : helper_(helper) {
    Block block = { FREE, 0, size, kUnusedToken };
    blocks_.push_back(block);
  }

  ~FencedAllocator();

  // Allocates a block of memory. If the buffer is out of directly available
  // memory, this function may wait until memory that was freed "pending a
  // token" can be re-used.
  //
  // Parameters:
  //   size: the size of the memory block to allocate.
  //
  // Returns:
  //   the offset of the allocated memory block, or kInvalidOffset if out of
  //   memory.
  Offset Alloc(unsigned int size);

  // Frees a block of memory.
  //
  // Parameters:
  //   offset: the offset of the memory block to free.
  void Free(Offset offset);

  // Frees a block of memory, pending the passage of a token. That memory won't
  // be re-allocated until the token has passed through the command stream.
  //
  // Parameters:
  //   offset: the offset of the memory block to free.
  //   token: the token value to wait for before re-using the memory.
  void FreePendingToken(Offset offset, unsigned int token);

  // Gets the size of the largest free block that is available without waiting.
  unsigned int GetLargestFreeSize();

  // Gets the size of the largest free block that can be allocated if the
  // caller can wait. Allocating a block of this size will succeed, but may
  // block.
  unsigned int GetLargestFreeOrPendingSize();

  // Checks for consistency inside the book-keeping structures. Used for
  // testing.
  bool CheckConsistency();

 private:
  // Status of a block of memory, for book-keeping.
  enum State {
    IN_USE,
    FREE,
    FREE_PENDING_TOKEN
  };

  // Book-keeping sturcture that describes a block of memory.
  struct Block {
    State state;
    Offset offset;
    unsigned int size;
    unsigned int token;  // token to wait for in the FREE_PENDING_TOKEN case.
  };

  // Comparison functor for memory block sorting.
  class OffsetCmp {
   public:
    bool operator() (const Block &left, const Block &right) {
      return left.offset < right.offset;
    }
  };

  typedef std::vector<Block> Container;
  typedef unsigned int BlockIndex;

  static const unsigned int kUnusedToken = 0;

  // Gets the index of a memory block, given its offset.
  BlockIndex GetBlockByOffset(Offset offset);

  // Collapse a free block with its neighbours if they are free. Returns the
  // index of the collapsed block.
  // NOTE: this will invalidate block indices.
  BlockIndex CollapseFreeBlock(BlockIndex index);

  // Waits for a FREE_PENDING_TOKEN block to be usable, and free it. Returns
  // the new index of that block (since it may have been collapsed).
  // NOTE: this will invalidate block indices.
  BlockIndex WaitForTokenAndFreeBlock(BlockIndex index);

  // Allocates a block of memory inside a given block, splitting it in two
  // (unless that block is of the exact requested size).
  // NOTE: this will invalidate block indices.
  // Returns the offset of the allocated block (NOTE: this is different from
  // the other functions that return a block index).
  Offset AllocInBlock(BlockIndex index, unsigned int size);

  command_buffer::CommandBufferHelper *helper_;
  Container blocks_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(FencedAllocator);
};

// This class functions just like FencedAllocator, but its API uses pointers
// instead of offsets.
class FencedAllocatorWrapper {
 public:
  FencedAllocatorWrapper(unsigned int size,
                         CommandBufferHelper *helper,
                         void *base)
      : allocator_(size, helper),
        base_(base) { }

  // Allocates a block of memory. If the buffer is out of directly available
  // memory, this function may wait until memory that was freed "pending a
  // token" can be re-used.
  //
  // Parameters:
  //   size: the size of the memory block to allocate.
  //
  // Returns:
  //   the pointer to the allocated memory block, or NULL if out of
  //   memory.
  void *Alloc(unsigned int size) {
    FencedAllocator::Offset offset = allocator_.Alloc(size);
    return GetPointer(offset);
  }

  // Allocates a block of memory. If the buffer is out of directly available
  // memory, this function may wait until memory that was freed "pending a
  // token" can be re-used.
  // This is a type-safe version of Alloc, returning a typed pointer.
  //
  // Parameters:
  //   count: the number of elements to allocate.
  //
  // Returns:
  //   the pointer to the allocated memory block, or NULL if out of
  //   memory.
  template <typename T> T *AllocTyped(unsigned int count) {
    return static_cast<T *>(Alloc(count * sizeof(T)));
  }

  // Frees a block of memory.
  //
  // Parameters:
  //   pointer: the pointer to the memory block to free.
  void Free(void *pointer) {
    DCHECK(pointer);
    allocator_.Free(GetOffset(pointer));
  }

  // Frees a block of memory, pending the passage of a token. That memory won't
  // be re-allocated until the token has passed through the command stream.
  //
  // Parameters:
  //   pointer: the pointer to the memory block to free.
  //   token: the token value to wait for before re-using the memory.
  void FreePendingToken(void *pointer, unsigned int token) {
    DCHECK(pointer);
    allocator_.FreePendingToken(GetOffset(pointer), token);
  }

  // Gets a pointer to a memory block given the base memory and the offset.
  // It translates FencedAllocator::kInvalidOffset to NULL.
  void *GetPointer(FencedAllocator::Offset offset) {
    return (offset == FencedAllocator::kInvalidOffset) ?
        NULL : static_cast<char *>(base_) + offset;
  }

  // Gets the offset to a memory block given the base memory and the address.
  // It translates NULL to FencedAllocator::kInvalidOffset.
  FencedAllocator::Offset GetOffset(void *pointer) {
    return pointer ? static_cast<char *>(pointer) - static_cast<char *>(base_) :
        FencedAllocator::kInvalidOffset;
  }

  // Gets the size of the largest free block that is available without waiting.
  unsigned int GetLargestFreeSize() {
    return allocator_.GetLargestFreeSize();
  }

  // Gets the size of the largest free block that can be allocated if the
  // caller can wait.
  unsigned int GetLargestFreeOrPendingSize() {
    return allocator_.GetLargestFreeOrPendingSize();
  }

  // Checks for consistency inside the book-keeping structures. Used for
  // testing.
  bool CheckConsistency() {
    return allocator_.CheckConsistency();
  }

  FencedAllocator &allocator() { return allocator_; }

 private:
  FencedAllocator allocator_;
  void *base_;
  DISALLOW_IMPLICIT_CONSTRUCTORS(FencedAllocatorWrapper);
};

}  // namespace command_buffer
}  // namespace o3d


#endif  // O3D_COMMAND_BUFFER_CLIENT_CROSS_FENCED_ALLOCATOR_H_
