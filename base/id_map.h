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

#ifndef BASE_ID_MAP_H__
#define BASE_ID_MAP_H__

// hash map is common (GCC4 and Dinkumware also support it, for example), but
// is not strictly part of the C++ standard. MS puts it into a funny namespace,
// although most other vendors seem to use std. This may have to change if
// other platforms are supported.
#include <hash_map>
using stdext::hash_map;

#include "base/basictypes.h"
#include "base/logging.h"

// This object maintains a list of IDs that can be quickly converted to
// pointers to objects. It is implemented as a hash table, optimized for
// relatively small data sets (in the common case, there will be exactly one
// item in the list).
//
// Items can be inserted into the container with arbitrary ID, but the caller
// must ensure they are unique. Inserting IDs and relying on automatically
// generated ones is not allowed because they can collide.
template<class T>
class IDMap {
 private:
  typedef hash_map<int32, T*> HashTable;

 public:
  IDMap() : next_id_(1) {
  }
  IDMap(const IDMap& other) : next_id_(other.next_id_),
                                        data_(other.data_) {
  }

  // support const iterators over the items
  // Note, use iterator->first to get the ID, iterator->second to get the T*
  typedef typename HashTable::const_iterator const_iterator;
  const_iterator begin() const {
    return data_.begin();
  }
  const_iterator end() const {
    return data_.end();
  }

  // Adds a view with an automatically generated unique ID. See AddWithID.
  int32 Add(T* data) {
    int32 this_id = next_id_;
    DCHECK(data_.find(this_id) == data_.end()) << "Inserting duplicate item";
    data_[this_id] = data;
    next_id_++;
    return this_id;
  }

  // Adds a new data member with the specified ID. The ID must not be in
  // the list. The caller either must generate all unique IDs itself and use
  // this function, or allow this object to generate IDs and call Add. These
  // two methods may not be mixed, or duplicate IDs may be generated
  void AddWithID(T* data, int32 id) {
    DCHECK(data_.find(id) == data_.end()) << "Inserting duplicate item";
    data_[id] = data;
  }

  void Remove(int32 id) {
    HashTable::iterator i = data_.find(id);
    if (i == data_.end()) {
      NOTREACHED() << "Attempting to remove an item not in the list";
      return;
    }
    data_.erase(i);
  }

  bool IsEmpty() const {
    return data_.empty();
  }

  T* Lookup(int32 id) const {
    HashTable::const_iterator i = data_.find(id);
    if (i == data_.end())
      return NULL;
    return i->second;
  }

  size_t size() const {
    return data_.size();
  }

 protected:
  // The next ID that we will return from Add()
  int32 next_id_;

  HashTable data_;
};

#endif  // BASE_ID_MAP_H__
