// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ID_MAP_H__
#define BASE_ID_MAP_H__

#include "base/basictypes.h"
#include "base/hash_tables.h"
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
  typedef base::hash_map<int32, T*> HashTable;
  typedef typename HashTable::iterator iterator;

 public:
  // support const iterators over the items
  // Note, use iterator->first to get the ID, iterator->second to get the T*
  typedef typename HashTable::const_iterator const_iterator;

  IDMap() : next_id_(1), check_on_null_data_(false) {
  }
  IDMap(const IDMap& other)
      : next_id_(other.next_id_),
        data_(other.data_),
        check_on_null_data_(other.check_on_null_data_) {
  }

  // Sets whether Add should CHECK if passed in NULL data. Default is false.
  void set_check_on_null_data(bool value) { check_on_null_data_ = value; }

  const_iterator begin() const {
    return data_.begin();
  }
  const_iterator end() const {
    return data_.end();
  }

  // Adds a view with an automatically generated unique ID. See AddWithID.
  int32 Add(T* data) {
    CHECK(!check_on_null_data_ || data);
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
    CHECK(!check_on_null_data_ || data);
    DCHECK(data_.find(id) == data_.end()) << "Inserting duplicate item";
    data_[id] = data;
  }

  void Remove(int32 id) {
    iterator i = data_.find(id);
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
    const_iterator i = data_.find(id);
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

 private:
  // See description above setter.
  bool check_on_null_data_;
};

#endif  // BASE_ID_MAP_H__
