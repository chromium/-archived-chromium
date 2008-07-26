// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "sandbox/src/shared_handles.h"

namespace sandbox {

// Note once again the the assumption here is that the shared memory is
// initialized with zeros in the process that calls SetHandle and that
// the process that calls GetHandle 'sees' this memory.

SharedHandles::SharedHandles() {
  shared_.items = NULL;
  shared_.max_items = 0;
}

bool SharedHandles::Init(void* raw_mem, size_t size_bytes) {
  if (size_bytes < sizeof(shared_.items[0])) {
    // The shared memory is too small!
    return false;
  }
  shared_.items = static_cast<SharedItem*>(raw_mem);
  shared_.max_items = size_bytes / sizeof(shared_.items[0]);
  return true;
}

// Note that an empty slot is marked with a tag == 0 that is why is
// not a valid imput tag
bool SharedHandles::SetHandle(uint32 tag, HANDLE handle) {
  if (0 == tag) {
    // Invalid tag
    return false;
  }
  // Find empty slot and put the tag and the handle there
  SharedItem* empty_slot = FindByTag(0);
  if (NULL == empty_slot) {
    return false;
  }
  empty_slot->tag = tag;
  empty_slot->item = handle;
  return true;
}

bool SharedHandles::GetHandle(uint32 tag, HANDLE* handle) {
  if (0 == tag) {
    // Invalid tag
    return false;
  }
  SharedItem* found = FindByTag(tag);
  if (NULL == found) {
    return false;
  }
  *handle = found->item;
  return true;
}

SharedHandles::SharedItem* SharedHandles::FindByTag(uint32 tag) {
  for (size_t ix = 0; ix != shared_.max_items; ++ix) {
    if (tag == shared_.items[ix].tag) {
      return &shared_.items[ix];
    }
  }
  return NULL;
}

}  // namespace sandbox
