// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/visitedlink_slave.h"

#include "base/logging.h"
#include "base/shared_memory.h"

VisitedLinkSlave::VisitedLinkSlave() : shared_memory_(NULL) {
}
VisitedLinkSlave::~VisitedLinkSlave() {
  FreeTable();
}

// This function's job is to initialize the table with the given
// shared memory handle. This memory is mappend into the process.
bool VisitedLinkSlave::Init(base::SharedMemoryHandle shared_memory) {
  // since this function may be called again to change the table, we may need
  // to free old objects
  FreeTable();
  DCHECK(shared_memory_ == NULL && hash_table_ == NULL);

  // create the shared memory object
  shared_memory_ = new base::SharedMemory(shared_memory, true);
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
