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

#include "base/revocable_store.h"

#include "base/logging.h"

RevocableStore::Revocable::Revocable(RevocableStore* store)
    : store_reference_(store->owning_reference_) {
  // We AddRef() the owning reference.
  DCHECK(store_reference_->store());
  store_reference_->store()->Add(this);
}

RevocableStore::Revocable::~Revocable() {
  if (!revoked()) {
    // Notify the store of our destruction.
    --(store_reference_->store()->count_);
  }
}

RevocableStore::RevocableStore() : count_(0) {
  // Create a new owning reference.
  owning_reference_ = new StoreRef(this);
}

RevocableStore::~RevocableStore() {
  // Revoke all the items in the store.
  owning_reference_->set_store(NULL);
}

void RevocableStore::Add(Revocable* item) {
  DCHECK(!item->revoked());
  ++count_;
}

void RevocableStore::RevokeAll() {
  // We revoke all the existing items in the store and reset our count.
  owning_reference_->set_store(NULL);
  count_ = 0;

  // Then we create a new owning reference for new items that get added.
  // This Release()s the old owning reference, allowing it to be freed after
  // all the items that were in the store are eventually destroyed.
  owning_reference_ = new StoreRef(this);
}
