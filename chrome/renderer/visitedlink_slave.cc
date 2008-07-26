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

#include <windows.h>
#include "base/logging.h"
#include "base/shared_memory.h"
#include "chrome/common/win_util.h"
#include "chrome/renderer/visitedlink_slave.h"

VisitedLinkSlave::VisitedLinkSlave() : shared_memory_(NULL) {
}
VisitedLinkSlave::~VisitedLinkSlave() {
  FreeTable();
}

// This function's job is to initialize the table with the given
// shared memory handle. This memory is mappend into the process.
bool VisitedLinkSlave::Init(SharedMemoryHandle shared_memory) {
  // since this function may be called again to change the table, we may need
  // to free old objects
  FreeTable();
  DCHECK(shared_memory_ == NULL && hash_table_ == NULL);

  // create the shared memory object
  shared_memory_ = new SharedMemory(shared_memory, true);
  if (!shared_memory_)
    return false;

  // map the header into our process so we can see how long the rest is,
  // and set the salt
  if (!shared_memory_->Map(sizeof(SharedHeader)))
    return false;
  SharedHeader* header =
    static_cast<SharedHeader*>(shared_memory_->memory());
  DCHECK(header);
  int32 table_len = header->length;
  memcpy(salt_, header->salt, sizeof(salt_));
  shared_memory_->Unmap();

  // now do the whole table because we know the length
  if (!shared_memory_->Map(sizeof(SharedHeader) +
                          table_len * sizeof(Fingerprint))) {
    shared_memory_->Close();
    return false;
  }

  // commit the data
  DCHECK(shared_memory_->memory());
  hash_table_ = reinterpret_cast<Fingerprint*>(
      static_cast<char*>(shared_memory_->memory()) + sizeof(SharedHeader));
  table_length_ = table_len;
  return true;
}

void VisitedLinkSlave::FreeTable() {
  if (shared_memory_ ) {
    delete shared_memory_;
    shared_memory_ = NULL;
  }
  hash_table_ = NULL;
  table_length_ = 0;
}
