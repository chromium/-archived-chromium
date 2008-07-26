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
  virtual Time GetLastUsed() const;
  virtual Time GetLastModified() const;
  virtual int32 GetDataSize(int index) const;
  virtual int ReadData(int index, int offset, char* buf, int buf_len,
                       net::CompletionCallback* completion_callback);
  virtual int WriteData(int index, int offset, const char* buf, int buf_len,
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
  ~MemEntryImpl();

  // Grows and cleans up the data buffer.
  void PrepareTarget(int index, int offset, int buf_len);

  // Updates ranking information.
  void UpdateRank(bool modified);

  std::string key_;
  std::vector<char> data_[2];  // User data.
  int32 data_size_[2];
  int ref_count_;

  MemEntryImpl* next_;         // Pointers for the LRU list.
  MemEntryImpl* prev_;
  Time last_modified_;         // LRU information.
  Time last_used_;
  MemBackendImpl* backend_;    // Back pointer to the cache.
  bool doomed_;                // True if this entry was removed from the cache.

  DISALLOW_EVIL_CONSTRUCTORS(MemEntryImpl);
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_MEM_ENTRY_IMPL_H__
