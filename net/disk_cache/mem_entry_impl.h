// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_MEM_ENTRY_IMPL_H_
#define NET_DISK_CACHE_MEM_ENTRY_IMPL_H_

#include "net/disk_cache/disk_cache.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

namespace disk_cache {

class MemBackendImpl;

// This class implements the Entry interface for the memory-only cache. An
// object of this class represents a single entry on the cache. We use two
// types of entries, parent and child to support sparse caching.
// A parent entry is non-sparse until a sparse method is invoked (i.e.
// ReadSparseData, WriteSparseData, GetAvailableRange) when sparse information
// is initialized. It then manages a list of child entries and delegates the
// sparse API calls to the child entries. It creates and deletes child entries
// and updates the list when needed.
// A child entry is used to carry partial cache content, non-sparse methods like
// ReadData and WriteData cannot be applied to them. The lifetime of a child
// entry is managed by the parent entry that created it except that the entry
// can be evicted independently. A child entry does not have a key and it is not
// registered in the backend's entry map. It is registered in the backend's
// ranking list to enable eviction of a partial content.
class MemEntryImpl : public Entry {
 public:
  enum EntryType {
    kParentEntry,
    kChildEntry,
  };

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
  virtual int ReadSparseData(int64 offset, net::IOBuffer* buf, int buf_len,
                             net::CompletionCallback* completion_callback);
  virtual int WriteSparseData(int64 offset, net::IOBuffer* buf, int buf_len,
                              net::CompletionCallback* completion_callback);
  virtual int GetAvailableRange(int64 offset, int len, int64* start);

  // Performs the initialization of a EntryImpl that will be added to the
  // cache.
  bool CreateEntry(const std::string& key);

  // Performs the initialization of a MemEntryImpl as a child entry.
  // TODO(hclam): this method should be private. Leave this as public because
  // this is the only way to create a child entry. Move this method to private
  // once child entries are created by parent entry.
  bool CreateChildEntry(MemEntryImpl* parent);

  // Permanently destroys this entry.
  void InternalDoom();

  void Open();
  bool InUse();

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

  EntryType type() const {
    return type_;
  }

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
  MemEntryImpl* parent_;       // Pointer to the parent entry.
  base::Time last_modified_;   // LRU information.
  base::Time last_used_;
  MemBackendImpl* backend_;    // Back pointer to the cache.
  bool doomed_;                // True if this entry was removed from the cache.
  EntryType type_;             // The type of this entry.

  DISALLOW_EVIL_CONSTRUCTORS(MemEntryImpl);
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_MEM_ENTRY_IMPL_H_
