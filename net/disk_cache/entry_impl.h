// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_ENTRY_IMPL_H_
#define NET_DISK_CACHE_ENTRY_IMPL_H_

#include "base/scoped_ptr.h"
#include "net/disk_cache/disk_cache.h"
#include "net/disk_cache/storage_block.h"
#include "net/disk_cache/storage_block-inl.h"

namespace disk_cache {

class BackendImpl;
class SparseControl;

// This class implements the Entry interface. An object of this
// class represents a single entry on the cache.
class EntryImpl : public Entry, public base::RefCounted<EntryImpl> {
  friend class base::RefCounted<EntryImpl>;
  friend class SparseControl;
 public:
  EntryImpl(BackendImpl* backend, Addr address);

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

  inline CacheEntryBlock* entry() {
    return &entry_;
  }

  inline CacheRankingsBlock* rankings() {
    return &node_;
  }

  uint32 GetHash();

  // Performs the initialization of a EntryImpl that will be added to the
  // cache.
  bool CreateEntry(Addr node_address, const std::string& key, uint32 hash);

  // Returns true if this entry matches the lookup arguments.
  bool IsSameEntry(const std::string& key, uint32 hash);

  // Permamently destroys this entry.
  void InternalDoom();

  // Deletes this entry from disk. If |everything| is false, only the user data
  // will be removed, leaving the key and control data intact.
  void DeleteEntryData(bool everything);

  // Returns the address of the next entry on the list of entries with the same
  // hash.
  CacheAddr GetNextAddress();

  // Sets the address of the next entry on the list of entries with the same
  // hash.
  void SetNextAddress(Addr address);

  // Reloads the rankings node information.
  bool LoadNodeAddress();

  // Updates the stored data to reflect the run-time information for this entry.
  // Returns false if the data could not be updated. The purpose of this method
  // is to be able to detect entries that are currently in use.
  bool Update();

  // Returns true if this entry is marked as dirty on disk.
  bool IsDirty(int32 current_id);
  void ClearDirtyFlag();

  // Fixes this entry so it can be treated as valid (to delete it).
  void SetPointerForInvalidEntry(int32 new_id);

  // Returns false if the entry is clearly invalid.
  bool SanityCheck();

  // Handle the pending asynchronous IO count.
  void IncrementIoCount();
  void DecrementIoCount();

  // Set the access times for this entry. This method provides support for
  // the upgrade tool.
  void SetTimes(base::Time last_used, base::Time last_modified);

 private:
  enum {
     kNumStreams = 3
  };

  enum Operation {
    kRead,
    kWrite,
    kSparseRead,
    kSparseWrite
  };

  ~EntryImpl();

  // Initializes the storage for an internal or external data block.
  bool CreateDataBlock(int index, int size);

  // Initializes the storage for an internal or external generic block.
  bool CreateBlock(int size, Addr* address);

  // Deletes the data pointed by address, maybe backed by files_[index].
  void DeleteData(Addr address, int index);

  // Updates ranking information.
  void UpdateRank(bool modified);

  // Returns a pointer to the file that stores the given address.
  File* GetBackingFile(Addr address, int index);

  // Returns a pointer to the file that stores external data.
  File* GetExternalFile(Addr address, int index);

  // Prepares the target file or buffer for a write of buf_len bytes at the
  // given offset.
  bool PrepareTarget(int index, int offset, int buf_len, bool truncate);

  // Grows the size of the storage used to store user data, if needed.
  bool GrowUserBuffer(int index, int offset, int buf_len, bool truncate);

  // Reads from a block data file to this object's memory buffer.
  bool MoveToLocalBuffer(int index);

  // Loads the external file to this object's memory buffer.
  bool ImportSeparateFile(int index, int offset, int buf_len);

  // Flush the in-memory data to the backing storage.
  bool Flush(int index, int size, bool async);

  // Initializes the sparse control object. Returns a net error code.
  int InitSparseData();

  // Generates a histogram for the time spent working on this operation.
  void ReportIOTime(Operation op, const base::Time& start);

  // Logs this entry to the internal trace buffer.
  void Log(const char* msg);

  CacheEntryBlock entry_;     // Key related information for this entry.
  CacheRankingsBlock node_;   // Rankings related information for this entry.
  BackendImpl* backend_;      // Back pointer to the cache.
  scoped_array<char> user_buffers_[kNumStreams];  // Store user data.
  scoped_refptr<File> files_[kNumStreams + 1];    // Files to store external
                                                  // user data and key.
  int unreported_size_[kNumStreams];  // Bytes not reported yet to the backend.
  bool doomed_;               // True if this entry was removed from the cache.
  scoped_ptr<SparseControl> sparse_;  // Support for sparse entries.

  DISALLOW_EVIL_CONSTRUCTORS(EntryImpl);
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_ENTRY_IMPL_H_
