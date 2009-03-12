// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/mem_backend_impl.h"

#include "base/logging.h"
#include "base/sys_info.h"
#include "net/disk_cache/cache_util.h"
#include "net/disk_cache/mem_entry_impl.h"

using base::Time;

namespace {

const int kDefaultCacheSize = 10 * 1024 * 1024;
const int kCleanUpMargin = 1024 * 1024;

int LowWaterAdjust(int high_water) {
  if (high_water < kCleanUpMargin)
    return 0;

  return high_water - kCleanUpMargin;
}

}  // namespace

namespace disk_cache {

Backend* CreateInMemoryCacheBackend(int max_bytes) {
  MemBackendImpl* cache = new MemBackendImpl();
  cache->SetMaxSize(max_bytes);
  if (cache->Init())
    return cache;

  delete cache;
  LOG(ERROR) << "Unable to create cache";
  return NULL;
}

// ------------------------------------------------------------------------

bool MemBackendImpl::Init() {
  if (max_size_)
    return true;

  int64 total_memory = base::SysInfo::AmountOfPhysicalMemory();

  if (total_memory <= 0) {
    max_size_ = kDefaultCacheSize;
    return true;
  }

  // We want to use up to 2% of the computer's memory, with a limit of 50 MB,
  // reached on systemd with more than 2.5 GB of RAM.
  total_memory = total_memory * 2 / 100;
  if (total_memory > kDefaultCacheSize * 5)
    max_size_ = kDefaultCacheSize * 5;
  else
    max_size_ = static_cast<int32>(total_memory);

  return true;
}

MemBackendImpl::~MemBackendImpl() {
  EntryMap::iterator it = entries_.begin();
  while (it != entries_.end()) {
    it->second->Doom();
    it = entries_.begin();
  }
  DCHECK(!current_size_);
}

bool MemBackendImpl::SetMaxSize(int max_bytes) {
  COMPILE_ASSERT(sizeof(max_bytes) == sizeof(max_size_), unsupported_int_model);
  if (max_bytes < 0)
    return false;

  // Zero size means use the default.
  if (!max_bytes)
    return true;

  max_size_ = max_bytes;
  return true;
}

int32 MemBackendImpl::GetEntryCount() const {
  return static_cast<int32>(entries_.size());
}

bool MemBackendImpl::OpenEntry(const std::string& key, Entry** entry) {
  EntryMap::iterator it = entries_.find(key);
  if (it == entries_.end())
    return false;

  it->second->Open();

  *entry = it->second;
  return true;
}

bool MemBackendImpl::CreateEntry(const std::string& key, Entry** entry) {
  EntryMap::iterator it = entries_.find(key);
  if (it != entries_.end())
    return false;

  MemEntryImpl* cache_entry = new MemEntryImpl(this);
  if (!cache_entry->CreateEntry(key)) {
    delete entry;
    return false;
  }

  rankings_.Insert(cache_entry);
  entries_[key] = cache_entry;

  *entry = cache_entry;
  return true;
}

bool MemBackendImpl::DoomEntry(const std::string& key) {
  Entry* entry;
  if (!OpenEntry(key, &entry))
    return false;

  entry->Doom();
  entry->Close();
  return true;
}

void MemBackendImpl::InternalDoomEntry(MemEntryImpl* entry) {
  rankings_.Remove(entry);
  EntryMap::iterator it = entries_.find(entry->GetKey());
  if (it != entries_.end())
    entries_.erase(it);
  else
    NOTREACHED();

  entry->InternalDoom();
}

bool MemBackendImpl::DoomAllEntries() {
  TrimCache(true);
  return true;
}

bool MemBackendImpl::DoomEntriesBetween(const Time initial_time,
                                        const Time end_time) {
  if (end_time.is_null())
    return DoomEntriesSince(initial_time);

  DCHECK(end_time >= initial_time);

  MemEntryImpl* next = rankings_.GetNext(NULL);

  // rankings_ is ordered by last used, this will descend through the cache
  // and start dooming items before the end_time, and will stop once it reaches
  // an item used before the initial time.
  while (next) {
    MemEntryImpl* node = next;
    next = rankings_.GetNext(next);

    if (node->GetLastUsed() < initial_time)
      break;

    if (node->GetLastUsed() < end_time) {
      node->Doom();
    }
  }

  return true;
}

// We use OpenNextEntry to retrieve elements from the cache, until we get
// entries that are too old.
bool MemBackendImpl::DoomEntriesSince(const Time initial_time) {
  for (;;) {
    Entry* entry;
    void* iter = NULL;
    if (!OpenNextEntry(&iter, &entry))
      return true;

    if (initial_time > entry->GetLastUsed()) {
      entry->Close();
      EndEnumeration(&iter);
      return true;
    }

    entry->Doom();
    entry->Close();
    EndEnumeration(&iter);  // Dooming the entry invalidates the iterator.
  }
}

bool MemBackendImpl::OpenNextEntry(void** iter, Entry** next_entry) {
  MemEntryImpl* current = reinterpret_cast<MemEntryImpl*>(*iter);
  MemEntryImpl* node = rankings_.GetNext(current);
  *next_entry = node;
  *iter = node;

  if (node)
    node->Open();

  return NULL != node;
}

void MemBackendImpl::EndEnumeration(void** iter) {
  *iter = NULL;
}

void MemBackendImpl::TrimCache(bool empty) {
  MemEntryImpl* next = rankings_.GetPrev(NULL);

  DCHECK(next);

  int target_size = empty ? 0 : LowWaterAdjust(max_size_);
  while (current_size_ > target_size && next) {
    MemEntryImpl* node = next;
    next = rankings_.GetPrev(next);
    if (!node->InUse() || empty) {
      node->Doom();
    }
  }

  return;
}

void MemBackendImpl::AddStorageSize(int32 bytes) {
  current_size_ += bytes;
  DCHECK(current_size_ >= 0);

  if (current_size_ > max_size_)
    TrimCache(false);
}

void MemBackendImpl::SubstractStorageSize(int32 bytes) {
  current_size_ -= bytes;
  DCHECK(current_size_ >= 0);
}

void MemBackendImpl::ModifyStorageSize(int32 old_size, int32 new_size) {
  if (old_size >= new_size)
    SubstractStorageSize(old_size - new_size);
  else
    AddStorageSize(new_size - old_size);
}

void MemBackendImpl::UpdateRank(MemEntryImpl* node) {
  rankings_.UpdateRank(node);
}

int MemBackendImpl::MaxFileSize() const {
  return max_size_ / 8;
}

}  // namespace disk_cache
