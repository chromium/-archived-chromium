// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
}

void Eviction::TrimCache(bool empty) {
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
      EntryImpl* entry;
      bool dirty;
      if (backend_->NewEntry(Addr(node->Data()->contents), &entry, &dirty)) {
        Trace("NewEntry failed on Trim 0x%x", node->address().value());
        continue;
      }

      if (node->Data()->pointer) {
        entry = EntryImpl::Update(entry);
      }
      ReportTrimTimes(entry);
      entry->Doom();
      entry->Release();
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

  UMA_HISTOGRAM_TIMES(L"DiskCache.TotalTrimTime", Time::Now() - start);
  Trace("*** Trim Cache end ***");
  return;
}

void Eviction::UpdateRank(EntryImpl* entry, bool modified) {
  rankings_->UpdateRank(entry->rankings(), modified, GetListForEntry(entry));
}

void Eviction::OnOpenEntry(EntryImpl* entry) {
}

void Eviction::OnCreateEntry(EntryImpl* entry) {
  rankings_->Insert(entry->rankings(), true, GetListForEntry(entry));
}

void Eviction::OnDoomEntry(EntryImpl* entry) {
  rankings_->Remove(entry->rankings(), GetListForEntry(entry));
}

void Eviction::OnDestroyEntry(EntryImpl* entry) {
}

void Eviction::ReportTrimTimes(EntryImpl* entry) {
  static bool first_time = true;
  if (first_time) {
    first_time = false;
    std::wstring name(StringPrintf(L"DiskCache.TrimAge_%d",
                                   header_->experiment));
    static Histogram counter(name.c_str(), 1, 10000, 50);
    counter.SetFlags(kUmaTargetedHistogramFlag);
    counter.Add((Time::Now() - entry->GetLastUsed()).InHours());
  }
}

Rankings::List Eviction::GetListForEntry(EntryImpl* entry) {
  return Rankings::NO_USE;
}

}  // namespace disk_cache
