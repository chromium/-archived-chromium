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


// This file contains the implementation of IdAllocator.

#include "command_buffer/client/cross/id_allocator.h"

namespace o3d {
namespace command_buffer {

IdAllocator::IdAllocator() : bitmap_(1) { bitmap_[0] = 0; }

static const unsigned int kBitsPerUint32 = sizeof(Uint32) * 8;  // NOLINT

// Looks for the first non-full entry, and return the first free bit in that
// entry. If all the entries are full, it will return the first bit of an entry
// that would be appended, but doesn't actually append that entry to the vector.
unsigned int IdAllocator::FindFirstFree() const {
  size_t size = bitmap_.size();
  for (unsigned int i = 0; i < size; ++i) {
    Uint32 value = bitmap_[i];
    if (value != 0xffffffffU) {
      for (unsigned int j = 0; j < kBitsPerUint32; ++j) {
        if (!(value & (1 << j))) return i * kBitsPerUint32 + j;
      }
      DLOG(FATAL) << "Code should not reach here.";
    }
  }
  return size*kBitsPerUint32;
}

// Sets the correct bit in the proper entry, resizing the vector if needed.
void IdAllocator::SetBit(unsigned int bit, bool value) {
  size_t size = bitmap_.size();
  if (bit >= size * kBitsPerUint32) {
    size_t newsize = bit / kBitsPerUint32 + 1;
    bitmap_.resize(newsize);
    for (size_t i = size; i < newsize; ++i) bitmap_[i] = 0;
  }
  Uint32 mask = 1U << (bit % kBitsPerUint32);
  if (value) {
    bitmap_[bit / kBitsPerUint32] |= mask;
  } else {
    bitmap_[bit / kBitsPerUint32] &= ~mask;
  }
}

// Gets the bit from the proper entry. This doesn't resize the vector, just
// returns false if the bit is beyond the last entry.
bool IdAllocator::GetBit(unsigned int bit) const {
  size_t size = bitmap_.size();
  if (bit / kBitsPerUint32 >= size) return false;
  Uint32 mask = 1U << (bit % kBitsPerUint32);
  return (bitmap_[bit / kBitsPerUint32] & mask) != 0;
}

}  // namespace command_buffer
}  // namespace o3d
