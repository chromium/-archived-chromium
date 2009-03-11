// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines the public interface of the disk cache. For more details see
// http://dev.chromium.org/developers/design-documents/disk-cache

#ifndef NET_DISK_CACHE_DISK_CACHE_H_
#define NET_DISK_CACHE_DISK_CACHE_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/platform_file.h"
#include "base/time.h"
#include "net/base/completion_callback.h"

namespace net {
class IOBuffer;
}

namespace disk_cache {

class Entry;
class Backend;

// Returns an instance of the Backend. path points to a folder where
// the cached data will be stored. This cache instance must be the only object
// that will be reading or writing files to that folder. The returned object
// should be deleted when not needed anymore. If force is true, and there is
// a problem with the cache initialization, the files will be deleted and a
// new set will be created. max_bytes is the maximum size the cache can grow to.
// If zero is passed in as max_bytes, the cache will determine the value to use
// based on the available disk space. The returned pointer can be NULL if a
// fatal error is found.
Backend* CreateCacheBackend(const std::wstring& path, bool force,
                            int max_bytes);

// Returns an instance of a Backend implemented only in memory. The returned
// object should be deleted when not needed anymore. max_bytes is the maximum
// size the cache can grow to. If zero is passed in as max_bytes, the cache will
// determine the value to use based on the available memory. The returned
// pointer can be NULL if a fatal error is found.
Backend* CreateInMemoryCacheBackend(int max_bytes);

// The root interface for a disk cache instance.
class Backend {
 public:
  virtual ~Backend() {}

  // Returns the number of entries in the cache.
  virtual int32 GetEntryCount() const = 0;

  // Opens an existing entry.  Upon success, the out param holds a pointer
  // to a Entry object representing the specified disk cache entry.
  // When the entry pointer is no longer needed, the Close method
  // should be called.
  virtual bool OpenEntry(const std::string& key, Entry** entry) = 0;

  // Creates a new entry.  Upon success, the out param holds a pointer
  // to a Entry object representing the newly created disk cache
  // entry.  When the entry pointer is no longer needed, the Close
  // method should be called.
  virtual bool CreateEntry(const std::string& key, Entry** entry) = 0;

  // Marks the entry, specified by the given key, for deletion.
  virtual bool DoomEntry(const std::string& key) = 0;

  // Marks all entries for deletion.
  virtual bool DoomAllEntries() = 0;

  // Marks a range of entries for deletion. This supports unbounded deletes in
  // either direction by using null Time values for either argument.
  virtual bool DoomEntriesBetween(const base::Time initial_time,
                                  const base::Time end_time) = 0;

  // Marks all entries accessed since initial_time for deletion.
  virtual bool DoomEntriesSince(const base::Time initial_time) = 0;

  // Enumerate the cache.  Initialize iter to NULL before calling this method
  // the first time.  That will cause the enumeration to start at the head of
  // the cache.  For subsequent calls, pass the same iter pointer again without
  // changing its value.  This method returns false when there are no more
  // entries to enumerate.  When the entry pointer is no longer needed, the
  // Close method should be called.
  //
  // NOTE: This method does not modify the last_used field of the entry,
  // and therefore it does not impact the eviction ranking of the entry.
  virtual bool OpenNextEntry(void** iter, Entry** next_entry) = 0;

  // Releases iter without returning the next entry. Whenever OpenNextEntry()
  // returns true, but the caller is not interested in continuing the
  // enumeration by calling OpenNextEntry() again, the enumeration must be
  // ended by calling this method with iter returned by OpenNextEntry().
  virtual void EndEnumeration(void** iter) = 0;

  // Return a list of cache statistics.
  virtual void GetStats(
      std::vector<std::pair<std::string, std::string> >* stats) = 0;
};

// This interface represents an entry in the disk cache.
class Entry {
 public:
  // Marks this cache entry for deletion.
  virtual void Doom() = 0;

  // Releases this entry. Calling this method does not cancel pending IO
  // operations on this entry. Even after the last reference to this object has
  // been released, pending completion callbacks may be invoked.
  virtual void Close() = 0;

  // Returns the key associated with this cache entry.
  virtual std::string GetKey() const = 0;

  // Returns the time when this cache entry was last used.
  virtual base::Time GetLastUsed() const = 0;

  // Returns the time when this cache entry was last modified.
  virtual base::Time GetLastModified() const = 0;

  // Returns the size of the cache data with the given index.
  virtual int32 GetDataSize(int index) const = 0;

  // Copies cache data into the given buffer of length |buf_len|.  If
  // completion_callback is null, then this call blocks until the read
  // operation is complete.  Otherwise, completion_callback will be
  // called on the current thread once the read completes.  Returns the
  // number of bytes read or a network error code. If a completion callback is
  // provided then it will be called if this function returns ERR_IO_PENDING,
  // and a reference to |buf| will be retained until the callback is called.
  // Note that the callback will be invoked in any case, even after Close has
  // been called; in other words, the caller may close this entry without
  // having to wait for all the callbacks, and still rely on the cleanup
  // performed from the callback code.
  virtual int ReadData(int index, int offset, net::IOBuffer* buf, int buf_len,
                       net::CompletionCallback* completion_callback) = 0;

  // Copies cache data from the given buffer of length |buf_len|.  If
  // completion_callback is null, then this call blocks until the write
  // operation is complete.  Otherwise, completion_callback will be
  // called on the current thread once the write completes.  Returns the
  // number of bytes written or a network error code. If a completion callback
  // is provided then it will be called if this function returns ERR_IO_PENDING,
  // and a reference to |buf| will be retained until the callback is called.
  // Note that the callback will be invoked in any case, even after Close has
  // been called; in other words, the caller may close this entry without
  // having to wait for all the callbacks, and still rely on the cleanup
  // performed from the callback code.
  // If truncate is true, this call will truncate the stored data at the end of
  // what we are writing here.
  virtual int WriteData(int index, int offset, net::IOBuffer* buf, int buf_len,
                        net::CompletionCallback* completion_callback,
                        bool truncate) = 0;

  // Prepares a target stream as an external file, returns a corresponding
  // base::PlatformFile if successful, returns base::kInvalidPlatformFileValue
  // if fails. If this call returns a valid base::PlatformFile value (i.e.
  // not base::kInvalidPlatformFileValue), there is no guarantee that the file
  // is truncated. Implementor can always return base::kInvalidPlatformFileValue
  // if external file is not available in that particular implementation.
  // Caller should never close the file handle returned by this method, since
  // the handle should be managed by the implementor of this class. Caller
  // should never save the handle for future use.
  // With a stream prepared as an external file, the stream would always be
  // kept in an external file since creation, even if the stream has 0 bytes.
  // So we need to be cautious about using this option for preparing a stream or
  // we will end up having a lot of empty cache files. Calling this method also
  // means that all data written to the stream will always be written to file
  // directly *without* buffering.
  virtual base::PlatformFile UseExternalFile(int index) = 0;

  // Returns a read file handle for the cache stream referenced by |index|.
  // Caller should never close the handle returned by this method and should
  // not save it for future use. The lifetime of the base::PlatformFile handle
  // is managed by the implementor of this class.
  virtual base::PlatformFile GetPlatformFile(int index) = 0;

 protected:
  virtual ~Entry() {}
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_DISK_CACHE_H_
