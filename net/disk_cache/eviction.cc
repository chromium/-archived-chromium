// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The eviction policy is a very simple pure LRU, so the elements at the end of
// the list are evicted until kCleanUpMargin free space is available. There is
// only one list in use (Rankings::NO_USE), and elements are sent to the front
// of the list whenever they are accessed.

// The new (in-development) eviction policy ads re-use as a factor to evict
// an entry. The story so far:

// Entries are linked on separate lists depending on how often they are used.
// When we see an element for the first time, it goes to the NO_USE list; if
// the object is reused later on, we move it to the LOW_USE list, until it is
// used kHighUse times, at which point it is moved to the HIGH_USE list.
// Whenever an element is evicted, we move it to the DELETED list so that if the
// element is accessed again, we remember the fact that it was already stored
// and maybe in the future we don't evict that element.

// When we have to evict an element, first we try to use the last element from
// the NO_USE list, then we move to the LOW_USE and only then we evict an entry
// from the HIGH_USE. We attempt to keep entries on the cache for at least
// kTargetTime hours (with frequently accessed items stored for longer periods),
// but if we cannot do that, we fall-back to keep each list roughly the same
// size so that we have a chance to see an element again and move it to another
// list.

#include "net/disk_cache/eviction.h"

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/time.h"
#include "net/disk_cache/backend_impl.h"
#include "net/disk_cache/entry_impl.h"
#include "net/disk_cache/trace.h"

using base::Time;

namespace {

const int kCleanUpMargin = 1024 * 1024;
const int kHighUse = 10;  // Reuse count to be on the HIGH_USE list.
const int kTargetTime = 24 * 7;  // Time to be evicted (hours since last use).

int LowWaterAdjust(int high_water) {
  if (high_water < kCleanUpMargin)
    return 0;

  return high_water - kCleanUpMargin;
}

}  // namespace

namespace disk_cache {

void Eviction::Init(BackendImpl* backend) {
  // We grab a bunch of info from the backend to make the code a little cleaner
  // when we're actually doing work.
  backend_ = backend;
  rankings_ = &backend->rankings_;
  header_ = &backend_->data_->header;
  max_size_ = LowWaterAdjust(backend_->max_size_);
  new_eviction_ = backend->new_eviction_;
}

void Eviction::TrimCache(bool empty) {
  if (new_eviction_)
    return TrimCacheV2(empty);

  Trace("*** Trim Cache ***");
  if (backend_->disabled_)
    return;

  Time start = Time::Now();
  Rankings::ScopedRankingsBlock node(rankings_);
  Rankings::ScopedRankingsBlock next(rankings_,
      rankings_->GetPrev(node.get(), Rankings::NO_USE));
  DCHECK(next.get());
  int target_size = empty ? 0 : max_size_;
  int deleted = 0;
  while (header_->num_bytes > target_size && next.get()) {
    node.reset(next.release());
    next.reset(rankings_->GetPrev(node.get(), Rankings::NO_USE));
    if (!node->Data()->pointer || empty) {
      // This entry is not being used by anybody.
      if (!EvictEntry(node.get(), empty))
        continue;

      if (!empty)
        backend_->OnEvent(Stats::TRIM_ENTRY);
      if (++deleted == 4 && !empty) {
#if defined(OS_WIN)
        MessageLoop::current()->PostTask(FROM_HERE,
            factory_.NewRunnableMethod(&Eviction::TrimCache, false));
        break;
#endif
      }
    }
  }

  UMA_HISTOGRAM_TIMES("DiskCache.TotalTrimTime", Time::Now() - start);
  Trace("*** Trim Cache end ***");
  return;
}

void Eviction::UpdateRank(EntryImpl* entry, bool modified) {
  if (new_eviction_)
    return UpdateRankV2(entry, modified);

  rankings_->UpdateRank(entry->rankings(), modified, GetListForEntry(entry));
}

void Eviction::OnOpenEntry(EntryImpl* entry) {
  if (new_eviction_)
    return OnOpenEntryV2(entry);
}

void Eviction::OnCreateEntry(EntryImpl* entry) {
  if (new_eviction_)
    return OnCreateEntryV2(entry);

  rankings_->Insert(entry->rankings(), true, GetListForEntry(entry));
}

void Eviction::OnDoomEntry(EntryImpl* entry) {
  if (new_eviction_)
    return OnDoomEntryV2(entry);

  rankings_->Remove(entry->rankings(), GetListForEntry(entry));
}

void Eviction::OnDestroyEntry(EntryImpl* entry) {
  if (new_eviction_)
    return OnDestroyEntryV2(entry);
}

void Eviction::ReportTrimTimes(EntryImpl* entry) {
  static bool first_time = true;
  if (first_time) {
    first_time = false;
    std::string name(StringPrintf("DiskCache.TrimAge_%d",
                                  header_->experiment));
    static Histogram counter(name.c_str(), 1, 10000, 50);
    counter.SetFlags(kUmaTargetedHistogramFlag);
    counter.Add((Time::Now() - entry->GetLastUsed()).InHours());
  }
}

Rankings::List Eviction::GetListForEntry(EntryImpl* entry) {
  return Rankings::NO_USE;
}

bool Eviction::EvictEntry(CacheRankingsBlock* node, bool empty) {
  EntryImpl* entry;
  bool dirty;
  if (backend_->NewEntry(Addr(node->Data()->contents), &entry, &dirty)) {
    Trace("NewEntry failed on Trim 0x%x", node->address().value());
    return false;
  }

  if (node->Data()->pointer) {
    entry = EntryImpl::Update(entry);
  }
  ReportTrimTimes(entry);
  if (empty || !new_eviction_) {
    entry->Doom();
  } else {
    entry->DeleteEntryData(false);
    EntryStore* info = entry->entry()->Data();
    DCHECK(ENTRY_NORMAL == info->state);

    rankings_->Remove(entry->rankings(), GetListForEntryV2(entry));
    info->state = ENTRY_EVICTED;
    entry->entry()->Store();
    rankings_->Insert(entry->rankings(), true, Rankings::DELETED);
    backend_->OnEvent(Stats::TRIM_ENTRY);
  }
  entry->Release();

  return true;
}

// -----------------------------------------------------------------------

void Eviction::TrimCacheV2(bool empty) {
  Trace("*** Trim Cache ***");
  if (backend_->disabled_)
    return;

  Time start = Time::Now();

  const int kListsToSearch = 3;
  Rankings::ScopedRankingsBlock next[kListsToSearch];
  int list = Rankings::LAST_ELEMENT;

  // Get a node from each list.
  for (int i = 0; i < kListsToSearch; i++) {
    bool done = false;
    next[i].set_rankings(rankings_);
    if (done)
      continue;
    next[i].reset(rankings_->GetPrev(NULL, static_cast<Rankings::List>(i)));
    if (!empty && NodeIsOldEnough(next[i].get(), i)) {
      list = static_cast<Rankings::List>(i);
      done = true;
    }
  }

  // If we are not meeting the time targets lets move on to list length.
  if (!empty && Rankings::LAST_ELEMENT == list)
    list = SelectListByLenght();

  if (empty)
    list = 0;

  Rankings::ScopedRankingsBlock node(rankings_);

  int target_size = empty ? 0 : max_size_;
  int deleted = 0;
  for (; list < kListsToSearch; list++) {
    while (header_->num_bytes > target_size && next[list].get()) {
      node.reset(next[list].release());
      next[list].reset(rankings_->GetPrev(node.get(),
                                          static_cast<Rankings::List>(list)));
      if (!node->Data()->pointer || empty) {
        // This entry is not being used by anybody.
        if (!EvictEntry(node.get(), empty))
          continue;

        if (++deleted == 4 && !empty) {
          MessageLoop::current()->PostTask(FROM_HERE,
              factory_.NewRunnableMethod(&Eviction::TrimCache, false));
          break;
        }
      }
    }
    if (!empty)
      list = kListsToSearch;
  }

  if (empty || header_->lru.sizes[Rankings::DELETED] > header_->num_entries / 4)
    MessageLoop::current()->PostTask(FROM_HERE,
        factory_.NewRunnableMethod(&Eviction::TrimDeleted, empty));

  UMA_HISTOGRAM_TIMES("DiskCache.TotalTrimTime", Time::Now() - start);
  Trace("*** Trim Cache end ***");
  return;
}

void Eviction::UpdateRankV2(EntryImpl* entry, bool modified) {
  rankings_->UpdateRank(entry->rankings(), modified, GetListForEntryV2(entry));
}

void Eviction::OnOpenEntryV2(EntryImpl* entry) {
  EntryStore* info = entry->entry()->Data();
  DCHECK(ENTRY_NORMAL == info->state);

  if (info->reuse_count < kint32max) {
    info->reuse_count++;
    entry->entry()->set_modified();

    // We may need to move this to a new list.
    if (1 == info->reuse_count) {
      rankings_->Remove(entry->rankings(), Rankings::NO_USE);
      rankings_->Insert(entry->rankings(), false, Rankings::LOW_USE);
      entry->entry()->Store();
    } else if (kHighUse == info->reuse_count) {
      rankings_->Remove(entry->rankings(), Rankings::LOW_USE);
      rankings_->Insert(entry->rankings(), false, Rankings::HIGH_USE);
      entry->entry()->Store();
    }
  }
}

void Eviction::OnCreateEntryV2(EntryImpl* entry) {
  EntryStore* info = entry->entry()->Data();
  switch (info->state) {
    case ENTRY_NORMAL: {
      DCHECK(!info->reuse_count);
      DCHECK(!info->refetch_count);
      break;
    };
    case ENTRY_EVICTED: {
      if (info->refetch_count < kint32max)
        info->refetch_count++;

      if (info->refetch_count > kHighUse && info->reuse_count < kHighUse) {
        info->reuse_count = kHighUse;
      } else {
        info->reuse_count++;
      }
      info->state = ENTRY_NORMAL;
      entry->entry()->Store();
      rankings_->Remove(entry->rankings(), Rankings::DELETED);
      break;
    };
    default:
      NOTREACHED();
  }

  rankings_->Insert(entry->rankings(), true, GetListForEntryV2(entry));
}

void Eviction::OnDoomEntryV2(EntryImpl* entry) {
  EntryStore* info = entry->entry()->Data();
  if (ENTRY_NORMAL != info->state)
    return;

  rankings_->Remove(entry->rankings(), GetListForEntryV2(entry));

  info->state = ENTRY_DOOMED;
  entry->entry()->Store();
  rankings_->Insert(entry->rankings(), true, Rankings::DELETED);
}

void Eviction::OnDestroyEntryV2(EntryImpl* entry) {
  rankings_->Remove(entry->rankings(), Rankings::DELETED);
}

Rankings::List Eviction::GetListForEntryV2(EntryImpl* entry) {
  EntryStore* info = entry->entry()->Data();
  DCHECK(ENTRY_NORMAL == info->state);

  if (!info->reuse_count)
    return Rankings::NO_USE;

  if (info->reuse_count < kHighUse)
    return Rankings::LOW_USE;

  return Rankings::HIGH_USE;
}

void Eviction::TrimDeleted(bool empty) {
}

bool Eviction::NodeIsOldEnough(CacheRankingsBlock* node, int list) {
  if (!node)
    return false;

  // If possible, we want to keep entries on each list at least kTargetTime
  // hours. Each successive list on the enumeration has 2x the target time of
  // the previous list.
  Time used = Time::FromInternalValue(node->Data()->last_used);
  int multiplier = 1 << list;
  return (Time::Now() - used).InHours() > kTargetTime * multiplier;
}

int Eviction::SelectListByLenght() {
  // Start by having each list to be roughly the same size.
  if (header_->lru.sizes[0] > header_->num_entries / 4)
    return 0;
  if (header_->lru.sizes[1] > header_->num_entries / 4)
    return 1;
  return 2;
}

}  // namespace disk_cache
