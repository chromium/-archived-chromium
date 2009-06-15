// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// See net/disk_cache/disk_cache.h for the public interface of the cache.

#ifndef NET_DISK_CACHE_MEM_BACKEND_IMPL_H__
#define NET_DISK_CACHE_MEM_BACKEND_IMPL_H__

#include "base/hash_tables.h"

#include "net/disk_cache/disk_cache.h"
#include "net/disk_cache/mem_rankings.h"

namespace disk_cache {

class MemEntryImpl;

// This class implements the Backend interface. An object of this class handles
// the operations of the cache without writing to disk.
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
  virtual bool DoomEntriesBetween(const base::Time initial_time,
                                  const base::Time end_time);
  virtual bool DoomEntriesSince(const base::Time initial_time);
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

  // Insert an MemEntryImpl into the ranking list. This method is only called
  // from MemEntryImpl to insert child entries. The reference can be removed
  // by calling RemoveFromRankingList(|entry|).
  void InsertIntoRankingList(MemEntryImpl* entry);

  // Remove |entry| from ranking list. This method is only called from
  // MemEntryImpl to remove a child entry from the ranking list.
  void RemoveFromRankingList(MemEntryImpl* entry);

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
