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


// This file contains the definition of the IdAllocator class.

#ifndef O3D_COMMAND_BUFFER_CLIENT_CROSS_ID_ALLOCATOR_H_
#define O3D_COMMAND_BUFFER_CLIENT_CROSS_ID_ALLOCATOR_H_

#include <vector>
#include "base/basictypes.h"
#include "command_buffer/common/cross/types.h"
#include "command_buffer/common/cross/resource.h"

namespace o3d {
namespace command_buffer {

// A class to manage the allocation of resource IDs. It uses a bitfield stored
// into a vector of unsigned ints.
class IdAllocator {
 public:
  IdAllocator();

  // Allocates a new resource ID.
  command_buffer::ResourceID AllocateID() {
    unsigned int bit = FindFirstFree();
    SetBit(bit, true);
    return bit;
  }

  // Frees a resource ID.
  void FreeID(command_buffer::ResourceID id) {
    SetBit(id, false);
  }

  // Checks whether or not a resource ID is in use.
  bool InUse(command_buffer::ResourceID id) {
    return GetBit(id);
  }
 private:
  void SetBit(unsigned int bit, bool value);
  bool GetBit(unsigned int bit) const;
  unsigned int FindFirstFree() const;

  std::vector<Uint32> bitmap_;
  DISALLOW_COPY_AND_ASSIGN(IdAllocator);
};

}  // namespace command_buffer
}  // namespace o3d

#endif  // O3D_COMMAND_BUFFER_CLIENT_CROSS_ID_ALLOCATOR_H_
