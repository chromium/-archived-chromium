// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file declares a HttpTransactionFactory implementation that can be
// layered on top of another HttpTransactionFactory to add HTTP caching.  The
// caching logic follows RFC 2616 (any exceptions are called out in the code).
//
// The HttpCache takes a disk_cache::Backend as a parameter, and uses that for
// the cache storage.
//
// See HttpTransactionFactory and HttpTransaction for more details.

#ifndef NET_HTTP_HTTP_CACHE_H_
#define NET_HTTP_HTTP_CACHE_H_

#include <list>
#include <set>

#include "base/basictypes.h"
#include "base/hash_tables.h"
#include "base/scoped_ptr.h"
#include "base/task.h"
#include "net/http/http_transaction_factory.h"

namespace disk_cache {
class Backend;
class Entry;
}

namespace net {

class HttpNetworkSession;
class HttpRequestInfo;
class HttpResponseInfo;
class ProxyService;

class HttpCache : public HttpTransactionFactory {
 public:
  ~HttpCache();

  // The cache mode of operation.
  enum Mode {
    // Normal mode just behaves like a standard web cache.
    NORMAL = 0,
    // Record mode caches everything for purposes of offline playback.
    RECORD,
    // Playback mode replays from a cache without considering any
    // standard invalidations.
    PLAYBACK
  };

  // The type of an HttpCache object, essentially describe what an HttpCache
  // object is for.
  enum Type {
    // An HttpCache object for common objects, e.g. html pages, images, fonts,
    // css files, js files and other common web resources.
    COMMON = 0,
    // A cache system for media file, e.g. video and audio files. These files
    // are huge and has special requirement for access.
    MEDIA
  };

  // Initialize the cache from the directory where its data is stored. The
  // disk cache is initialized lazily (by CreateTransaction) in this case. If
  // |cache_size| is zero, a default value will be calculated automatically.
  HttpCache(ProxyService* proxy_service,
            const std::wstring& cache_dir,
            int cache_size);

  // Initialize the cache from the directory where its data is stored. The
  // disk cache is initialized lazily (by CreateTransaction) in  this case. If
  // |cache_size| is zero, a default value will be calculated automatically.
  // Provide an existing HttpNetworkSession, the cache can construct a
  // network layer with a shared HttpNetworkSession in order for multiple
  // network layers to share information (e.g. authenication data).
  HttpCache(HttpNetworkSession* session, const std::wstring& cache_dir,
            int cache_size);

  // Initialize using an in-memory cache. The cache is initialized lazily
  // (by CreateTransaction) in this case. If |cache_size| is zero, a default
  // value will be calculated automatically.
  HttpCache(ProxyService* proxy_service, int cache_size);

  // Initialize the cache from its component parts, which is useful for
  // testing.  The lifetime of the network_layer and disk_cache are managed by
  // the HttpCache and will be destroyed using |delete| when the HttpCache is
  // destroyed.
  HttpCache(HttpTransactionFactory* network_layer,
            disk_cache::Backend* disk_cache);

  HttpTransactionFactory* network_layer() { return network_layer_.get(); }
  disk_cache::Backend* disk_cache() { return disk_cache_.get(); }

  // HttpTransactionFactory implementation:
  virtual HttpTransaction* CreateTransaction();
  virtual HttpCache* GetCache();
  virtual void Suspend(bool suspend);

  // Helper function for reading response info from the disk cache.
  static bool ReadResponseInfo(disk_cache::Entry* disk_entry,
                               HttpResponseInfo* response_info);

  // Helper function for writing response info into the disk cache.
  static bool WriteResponseInfo(disk_cache::Entry* disk_entry,
                                const HttpResponseInfo* response_info,
                                bool skip_transient_headers);

  // Get/Set the cache's mode.
  void set_mode(Mode value) { mode_ = value; }
  Mode mode() { return mode_; }

  void set_type(Type type) { type_ = type; }
  Type type() { return type_; }

 private:

  // Types --------------------------------------------------------------------

  class Transaction;
  friend class Transaction;

  typedef std::list<Transaction*> TransactionList;

  struct ActiveEntry {
    disk_cache::Entry* disk_entry;
    Transaction*       writer;
    TransactionList    readers;
    TransactionList    pending_queue;
    bool               will_process_pending_queue;
    bool               doomed;

    explicit ActiveEntry(disk_cache::Entry*);
    ~ActiveEntry();
  };

  typedef base::hash_map<std::string, ActiveEntry*> ActiveEntriesMap;
  typedef std::set<ActiveEntry*> ActiveEntriesSet;


  // Methods ------------------------------------------------------------------

  std::string GenerateCacheKey(const HttpRequestInfo*);
  void DoomEntry(const std::string& key);
  void FinalizeDoomedEntry(ActiveEntry* entry);
  ActiveEntry* FindActiveEntry(const std::string& key);
  ActiveEntry* ActivateEntry(const std::string& key, disk_cache::Entry*);
  void DeactivateEntry(ActiveEntry* entry);
  ActiveEntry* OpenEntry(const std::string& key);
  ActiveEntry* CreateEntry(const std::string& cache_key);
  void DestroyEntry(ActiveEntry* entry);
  int AddTransactionToEntry(ActiveEntry* entry, Transaction* trans);
  void DoneWithEntry(ActiveEntry* entry, Transaction* trans);
  void DoneWritingToEntry(ActiveEntry* entry, bool success);
  void DoneReadingFromEntry(ActiveEntry* entry, Transaction* trans);
  void ConvertWriterToReader(ActiveEntry* entry);
  void RemovePendingTransaction(Transaction* trans);
  void ProcessPendingQueue(ActiveEntry* entry);


  // Events (called via PostTask) ---------------------------------------------

  void OnProcessPendingQueue(ActiveEntry* entry);


  // Variables ----------------------------------------------------------------

  // used when lazily constructing the disk_cache_
  std::wstring disk_cache_dir_;

  Mode mode_;
  Type type_;

  scoped_ptr<HttpTransactionFactory> network_layer_;
  scoped_ptr<disk_cache::Backend> disk_cache_;

  // The set of active entries indexed by cache key
  ActiveEntriesMap active_entries_;

  // The set of doomed entries
  ActiveEntriesSet doomed_entries_;

  ScopedRunnableMethodFactory<HttpCache> task_factory_;

  bool in_memory_cache_;
  int cache_size_;

  typedef base::hash_map<std::string, int> PlaybackCacheMap;
  scoped_ptr<PlaybackCacheMap> playback_cache_map_;

  RevocableStore transactions_;

  DISALLOW_COPY_AND_ASSIGN(HttpCache);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_CACHE_H_
