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

#ifndef BASE_REVOCABLE_STORE_H__
#define BASE_REVOCABLE_STORE_H__

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/ref_counted.h"

// |RevocableStore| is a container of items that can be removed from the store.
class RevocableStore {
 public:
  // A |StoreRef| is used to link the |RevocableStore| to its items.  There is
  // one StoreRef per store, and each item holds a reference to it.  If the
  // store wishes to revoke its items, it sets |store_| to null.  Items are
  // permitted to release their reference to the |StoreRef| when they no longer
  // require the store.
 class StoreRef : public base::RefCounted<StoreRef> {
   public:
    StoreRef(RevocableStore* store) : store_(store) { }

    void set_store(RevocableStore* store) { store_ = store; }
    RevocableStore* store() const { return store_; }

   private:
    RevocableStore* store_;

    DISALLOW_EVIL_CONSTRUCTORS(StoreRef);
  };

  // An item in the store.  On construction, the object adds itself to the
  // store.
  class Revocable {
   public:
    Revocable(RevocableStore* store);
    ~Revocable();

    // This item has been revoked if it no longer has a pointer to the store.
    bool revoked() const { return !store_reference_->store(); }

  private:
    // We hold a reference to the store through this ref pointer.  We release
    // this reference on destruction.
    scoped_refptr<StoreRef> store_reference_;

    DISALLOW_EVIL_CONSTRUCTORS(Revocable);
  };

  RevocableStore();
  ~RevocableStore();

  // Revokes all the items in the store.
  void RevokeAll();

  // Returns true if there are no items in the store.
  bool empty() const { return count_ == 0; }

 private:
  friend class Revocable;

  // Adds an item to the store.  To add an item to the store, construct it
  // with a pointer to the store.
  void Add(Revocable* item);

  // This is the reference the unrevoked items in the store hold.
  scoped_refptr<StoreRef> owning_reference_;

  // The number of unrevoked items in the store.
  int count_;

  DISALLOW_EVIL_CONSTRUCTORS(RevocableStore);
};

#endif  // BASE_REVOCABLE_STORE_H__
