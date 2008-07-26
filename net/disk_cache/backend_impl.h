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

#ifndef NET_DISK_CACHE_BACKEND_IMPL_H__
#define NET_DISK_CACHE_BACKEND_IMPL_H__

#include "net/disk_cache/block_files.h"
#include "net/disk_cache/disk_cache.h"
#include "net/disk_cache/rankings.h"
#include "net/disk_cache/stats.h"
#include "net/disk_cache/trace.h"

class Timer;

namespace disk_cache {

// This class implements the Backend interface. An object of this
// class handles the operations of the cache for a particular profile.
class BackendImpl : public Backend {
 public:
  explicit BackendImpl(const std::wstring& path)
      : path_(path), init_(false), mask_(0), block_files_(path),
        unit_test_(false), restarted_(false), max_size_(0) {}
  // mask can be used to limit the usable size of the hash table, for testing.
  BackendImpl(const std::wstring& path, uint32 mask)
      : path_(path), init_(false), mask_(mask), block_files_(path),
        unit_test_(false), restarted_(false), max_size_(0) {}
  ~BackendImpl();

  // Performs general initialization for this current instance of the cache.
  bool Init();

  // Backend interface.
  virtual int32 GetEntryCount() const;
  virtual bool OpenEntry(const std::string& key, Entry** entry);
  virtual bool CreateEntry(const std::string& key, Entry** entry);
  virtual bool DoomEntry(const std::string& key);
  virtual bool DoomAllEntries();
  virtual bool DoomEntriesBetween(const Time initial_time,
                                  const Time end_time);
  virtual bool DoomEntriesSince(const Time initial_time);
  virtual bool OpenNextEntry(void** iter, Entry** next_entry);
  virtual void EndEnumeration(void** iter);
  virtual void GetStats(StatsItems* stats);

  // Sets the maximum size for the total amount of data stored by this instance.
  bool SetMaxSize(int max_bytes);

  // Returns the actual file used to store a given (non-external) address.
  MappedFile* File(Addr address) {
    if (disabled_)
      return NULL;
    return block_files_.GetFile(address);
  }

  // Creates a new storage block of size block_count.
  bool CreateBlock(FileType block_type, int block_count,
                   Addr* block_address);

  // Deletes a given storage block. deep set to true can be used to zero-fill
  // the related storage in addition of releasing the related block.
  void DeleteBlock(Addr block_address, bool deep);

  // Permanently deletes an entry.
  void InternalDoomEntry(EntryImpl* entry);

  // Returns the full name for an external storage file.
  std::wstring GetFileName(Addr address) const;

  // Creates an external storage file.
  bool CreateExternalFile(Addr* address);

  // Updates the ranking information for an entry.
  void UpdateRank(CacheRankingsBlock* node, bool modified);

  // This method must be called whenever an entry is released for the last time.
  void CacheEntryDestroyed();

  // Handles the pending asynchronous IO count.
  void IncrementIoCount();
  void DecrementIoCount();

  // Returns the id being used on this run of the cache.
  int32 GetCurrentEntryId();

  // A node was recovered from a crash, it may not be on the index, so this
  // method checks it and takes the appropriate action.
  void RecoveredEntry(CacheRankingsBlock* rankings);

  // Clears the counter of references to test handling of corruptions.
  void ClearRefCountForTest();

  // Sets internal parameters to enable unit testing mode.
  void SetUnitTestMode();

  // A user data block is being created, extended or truncated.
  void ModifyStorageSize(int32 old_size, int32 new_size);

  // Returns the maximum size for a file to reside on the cache.
  int MaxFileSize() const;

  // Logs requests that are denied due to being too big.
  void TooMuchStorageRequested(int32 size);

  // Called when an interesting event should be logged (counted).
  void OnEvent(Stats::Counters an_event);

  // Timer callback to calculate usage statistics.
  void OnStatsTimer();

  // Peforms a simple self-check, and returns the number of dirty items
  // or an error code (negative value).
  int SelfCheck();

  // Reports a critical error (and disables the cache).
  void CriticalError(int error);

 private:
  // Creates a new backing file for the cache index.
  bool CreateBackingStore(HANDLE file);
  bool InitBackingStore(bool* file_created);

  // Returns a given entry from the cache. The entry to match is determined by
  // key and hash, and the returned entry may be the matched one or it's parent
  // on the list of entries with the same hash (or bucket).
  EntryImpl* MatchEntry(const std::string& key, uint32 hash,
                         bool find_parent);

  // Deletes entries from the cache until the current size is below the limit.
  // If empty is true, the whole cache will be trimmed, regardless of being in
  // use.
  void TrimCache(bool empty);

  void DestroyInvalidEntry(Addr address, EntryImpl* entry);

  // Creates a new entry object and checks to see if it is dirty. Returns zero
  // on success, or a disk_cache error on failure.
  int NewEntry(Addr address, EntryImpl** entry, bool* dirty);

  // Part of the selt test. Returns the number or dirty entries, or an error.
  int CheckAllEntries();

  // Part of the self test. Returns false if the entry is corrupt.
  bool CheckEntry(EntryImpl* cache_entry);

  // Performs basic checks on the index file. Returns false on failure.
  bool CheckIndex();

  // Dumps current cache statistics to the log.
  void LogStats();

  // Deletes the cache and starts again.
  void RestartCache();

  // Handles the used storage count.
  void AddStorageSize(int32 bytes);
  void SubstractStorageSize(int32 bytes);

  // Update the number of referenced cache entries.
  void IncreaseNumRefs();
  void DecreaseNumRefs();

  void AdjustMaxCacheSize(int table_len);

  scoped_refptr<MappedFile> index_;  // The main cache index.
  std::wstring path_;  // Path to the folder used as backing storage.
  Index* data_;  // Pointer to the index data.
  BlockFiles block_files_;  // Set of files used to store all data.
  Rankings rankings_;  // Rankings to be able to trim the cache.
  uint32 mask_;  // Binary mask to map a hash to the hash table.
  int32 max_size_;  // Maximum data size for this instance.
  int num_refs_;  // Number of referenced cache entries.
  int max_refs_;  // Max number of eferenced cache entries.
  int num_pending_io_;  // Number of pending IO operations;
  bool init_;  // controls the initialization of the system.
  bool restarted_;
  bool unit_test_;
  bool disabled_;

  Stats stats_;  // Usage statistcs.
  Task* timer_task_;
  Timer* timer_;  // Usage timer.
  TraceObject trace_object_;  // Inits and destroys internal tracing.

  DISALLOW_EVIL_CONSTRUCTORS(BackendImpl);
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_BACKEND_IMPL_H__
