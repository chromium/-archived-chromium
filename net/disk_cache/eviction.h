// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_EVICTION_H_
#define NET_DISK_CACHE_EVICTION_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/task.h"
#include "net/disk_cache/disk_format.h"
#include "net/disk_cache/rankings.h"

namespace disk_cache {

class BackendImpl;
class EntryImpl;

// This class implements the eviction algorithm for the cache and it is tightly
// integrated with BackendImpl.
class Eviction {
 public:
  Eviction() : backend_(NULL), ALLOW_THIS_IN_INITIALIZER_LIST(factory_(this)) {}
  ~Eviction() {}

  void Init(BackendImpl* backend);

  // Deletes entries from the cache until the current size is below the limit.
  // If empty is true, the whole cache will be trimmed, regardless of being in
  // use.
  void TrimCache(bool empty);

  // Updates the ranking information for an entry.
  void UpdateRank(EntryImpl* entry, bool modified);

  // Notifications of interesting events for a given entry.
  void OnOpenEntry(EntryImpl* entry);
  void OnCreateEntry(EntryImpl* entry);
  void OnDoomEntry(EntryImpl* entry);
  void OnDestroyEntry(EntryImpl* entry);

 private:
  void PostDelayedTrim();
  void DelayedTrim();
  void ReportTrimTimes(EntryImpl* entry);
  Rankings::List GetListForEntry(EntryImpl* entry);
  bool EvictEntry(CacheRankingsBlock* node, bool empty);

  // We'll just keep for a while a separate set of methods that implement the
  // new eviction algorithm. This code will replace the original methods when
  // finished.
  void TrimCacheV2(bool empty);
  void UpdateRankV2(EntryImpl* entry, bool modified);
  void OnOpenEntryV2(EntryImpl* entry);
  void OnCreateEntryV2(EntryImpl* entry);
  void OnDoomEntryV2(EntryImpl* entry);
  void OnDestroyEntryV2(EntryImpl* entry);
  Rankings::List GetListForEntryV2(EntryImpl* entry);
  void TrimDeleted(bool empty);
  bool RemoveDeletedNode(CacheRankingsBlock* node);

  bool NodeIsOldEnough(CacheRankingsBlock* node, int list);
  int SelectListByLenght();
  void ReportListStats();

  BackendImpl* backend_;
  Rankings* rankings_;
  IndexHeader* header_;
  int max_size_;
  bool new_eviction_;
  bool first_trim_;
  bool trimming_;
  bool delay_trim_;
  ScopedRunnableMethodFactory<Eviction> factory_;

  DISALLOW_COPY_AND_ASSIGN(Eviction);
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_EVICTION_H_
