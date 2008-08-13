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

// See net/disk_cache/disk_cache.h for the public interface of the cache.

#ifndef NET_DISK_CACHE_MEM_BACKEND_IMPL_H__
#define NET_DISK_CACHE_MEM_BACKEND_IMPL_H__

#include "base/hash_tables.h"

#include "net/disk_cache/disk_cache.h"
#include "net/disk_cache/mem_rankings.h"

namespace disk_cache {

class MemEntryImpl;

// This class implements the Backend interface. An object of this class handles
// the operations of the cache without writting to disk.
class MemBackendImpl : public Backend {
 public:
  MemBackendImpl() : max_size_(0), current_size_(0) {}
  ~MemBackendImpl();

  // Performs general initialization for this current instance of the cache.
  bool Init();

  // Backend interface.
  virtual int32 GetEntryCount() const;
  virtual bool OpenEntry(const std::string& key, Entry** entry);
  virtual bool CreateEntry(const std::string& key, Entry** entry);
  virtual bool DoomEntry(const std::string& key);
  virtual bool DoomAllEntries();
  virtual bool DoomEntriesBetween(const Time initial_time, const Time end_time);
  virtual bool DoomEntriesSince(const Time initial_time);
  virtual bool OpenNextEntry(void** iter, Entry** next_entry);
  virtual void EndEnumeration(void** iter);
  virtual void GetStats(
      std::vector<std::pair<std::string, std::string> >* stats) {}

  // Sets the maximum size for the total amount of data stored by this instance.
  bool SetMaxSize(int max_bytes);

  // Permanently deletes an entry.
  void InternalDoomEntry(MemEntryImpl* entry);

  // Updates the ranking information for an entry.
  void UpdateRank(MemEntryImpl* node);

  // A user data block is being created, extended or truncated.
  void ModifyStorageSize(int32 old_size, int32 new_size);

  // Returns the maximum size for a file to reside on the cache.
  int MaxFileSize() const;

 private:
  // Deletes entries from the cache until the current size is below the limit.
  // If empty is true, the whole cache will be trimmed, regardless of being in
  // use.
  void TrimCache(bool empty);

  // Handles the used storage count.
  void AddStorageSize(int32 bytes);
  void SubstractStorageSize(int32 bytes);

  typedef base::hash_map<std::string, MemEntryImpl*> EntryMap;

  EntryMap entries_;
  MemRankings rankings_;  // Rankings to be able to trim the cache.
  int32 max_size_;        // Maximum data size for this instance.
  int32 current_size_;

  DISALLOW_EVIL_CONSTRUCTORS(MemBackendImpl);
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_MEM_BACKEND_IMPL_H__
