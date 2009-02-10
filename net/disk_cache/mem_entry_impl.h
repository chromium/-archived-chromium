// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_MEM_ENTRY_IMPL_H__
#define NET_DISK_CACHE_MEM_ENTRY_IMPL_H__

#include "net/disk_cache/disk_cache.h"

namespace disk_cache {

class MemBackendImpl;

// This class implements the Entry interface for the memory-only cache. An
// object of this class represents a single entry on the cache.
class MemEntryImpl : public Entry {
 public:
  explicit MemEntryImpl(MemBackendImpl* backend);

  // Entry interface.
  virtual void Doom();
  virtual void Close();
  virtual std::string GetKey() const;
  virtual base::Time GetLastUsed() const;
  virtual base::Time GetLastModified() const;
  virtual int32 GetDataSize(int index) const;
  virtual int ReadData(int index, int offset, net::IOBuffer* buf, int buf_len,
                       net::CompletionCallback* completion_callback);
  virtual int WriteData(int index, int offset, net::IOBuffer* buf, int buf_len,
                        net::CompletionCallback* completion_callback,
                        bool truncate);

  // Performs the initialization of a EntryImpl that will be added to the
  // cache.
  bool CreateEntry(const std::string& key);

  // Permamently destroys this entry
  void InternalDoom();

  MemEntryImpl* next() const {
    return next_;
  }

  MemEntryImpl* prev() const {
    return prev_;
  }

  void set_next(MemEntryImpl* next) {
    next_ = next;
  }

  void set_prev(MemEntryImpl* prev) {
    prev_ = prev;
  }

  void Open();
  bool InUse();

 private:
   enum {
     NUM_STREAMS = 3
   };

  ~MemEntryImpl();

  // Grows and cleans up the data buffer.
  void PrepareTarget(int index, int offset, int buf_len);

  // Updates ranking information.
  void UpdateRank(bool modified);

  std::string key_;
  std::vector<char> data_[NUM_STREAMS];  // User data.
  int32 data_size_[NUM_STREAMS];
  int ref_count_;

  MemEntryImpl* next_;         // Pointers for the LRU list.
  MemEntryImpl* prev_;
  base::Time last_modified_;         // LRU information.
  base::Time last_used_;
  MemBackendImpl* backend_;    // Back pointer to the cache.
  bool doomed_;                // True if this entry was removed from the cache.

  DISALLOW_EVIL_CONSTRUCTORS(MemEntryImpl);
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_MEM_ENTRY_IMPL_H__

