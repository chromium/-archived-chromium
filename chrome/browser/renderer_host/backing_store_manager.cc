// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/backing_store_manager.h"

#include "base/sys_info.h"
#include "chrome/browser/renderer_host/backing_store.h"
#include "chrome/browser/renderer_host/render_widget_host.h"
#include "chrome/browser/renderer_host/render_widget_host_painting_observer.h"
#include "chrome/common/chrome_constants.h"


namespace {

// There are two separate caches, |large_cache| and |small_cache|.  large_cache
// is meant for large items (tabs, popup windows), while small_cache is meant
// for small items (extension toolstrips and buttons, etc.).  The idea is that
// we'll almost always try to evict from large_cache first since small_cache
// items will tend to be visible more of the time.
typedef OwningMRUCache<RenderWidgetHost*, BackingStore*> BackingStoreCache;
static BackingStoreCache* large_cache = NULL;
static BackingStoreCache* small_cache = NULL;

// Threshold is based on a large-monitor width toolstrip.
// TODO(erikkay) 32bpp assumption isn't great.
const size_t kSmallThreshold = 4 * 32 * 1920;

// Previously, the backing store cache was based on a set number of backing
// stores, regardless of their size.  The numbers were chosen based on a user
// with a maximized browser on a large monitor.  Now that the cache is based on
// total memory size of the backing stores, we'll keep an approximation of the
// numbers from the previous algorithm by choosing a large monitor backing store
// size as our multiplier.
// TODO(erikkay) Perhaps we should actually use monitor size?  That way we
// could make an assertion like "worse case, there are two tabs in the cache".
// However, the small_cache might mess up these calculations a bit.
// TODO(erikkay) 32bpp assumption isn't great.
const size_t kMemoryMultiplier = 4 * 1920 * 1200;  // ~9MB

static size_t GetBackingStoreCacheMemorySize() {
  // Compute in terms of the number of large monitor's worth of backing-store.
  // Use a minimum of 2, and add one for each 256MB of physical memory you have.
  // Cap at 5, the thinking being that even if you have a gigantic amount of
  // RAM, there's a limit to how much caching helps beyond a certain number
  // of tabs.
  size_t mem_tier = std::min(5,
      2 + (base::SysInfo::AmountOfPhysicalMemoryMB() / 256));
  return mem_tier * kMemoryMultiplier;
}

// Expires the given |backing_store| from |cache|.
void ExpireBackingStoreAt(BackingStoreCache* cache,
                          BackingStoreCache::iterator backing_store) {
  RenderWidgetHost* rwh = backing_store->second->render_widget_host();
  if (rwh->painting_observer()) {
    rwh->painting_observer()->WidgetWillDestroyBackingStore(
        backing_store->first,
        backing_store->second);
  }
  cache->Erase(backing_store);
}

void CreateCacheSpace(size_t size) {
  // Given a request for |size|, first free from the large cache (until there's
  // only one item left) and then do the same from the small cache if we still
  // don't have enough.
  while (size > 0 && (large_cache->size() > 1 || small_cache->size() > 1)) {
    BackingStoreCache* cache =
        (large_cache->size() > 1) ? large_cache : small_cache;
    while (size > 0 && cache->size() > 1) {
      // Crazy C++ alert: rbegin.base() is a forward iterator pointing to end(),
      // so we need to do -- to move one back to the actual last item.
      BackingStoreCache::iterator entry = --cache->rbegin().base();
      size_t entry_size = entry->second->MemorySize();
      ExpireBackingStoreAt(cache, entry);
      if (size > entry_size)
        size -= entry_size;
      else
        size = 0;
    }
  }
  DCHECK(size == 0);
}

// Creates the backing store for the host based on the dimensions passed in.
// Removes the existing backing store if there is one.
BackingStore* CreateBackingStore(RenderWidgetHost* host,
                                 const gfx::Size& backing_store_size) {
  // Remove any existing backing store in case we're replacing it.
  BackingStoreManager::RemoveBackingStore(host);

  if (!large_cache) {
    large_cache = new BackingStoreCache(BackingStoreCache::NO_AUTO_EVICT);
    small_cache = new BackingStoreCache(BackingStoreCache::NO_AUTO_EVICT);
  }

  // TODO(erikkay) 32bpp is not always accurate
  size_t new_mem = backing_store_size.GetArea() * 4;
  size_t current_mem = BackingStoreManager::MemorySize();
  size_t max_mem = GetBackingStoreCacheMemorySize();
  DCHECK(new_mem < max_mem);
  if (current_mem + new_mem > max_mem) {
    // Need to remove old backing stores to make room for the new one. We
    // don't want to do this when the backing store is being replace by a new
    // one for the same tab, but this case won't get called then: we'll have
    // removed the onld one in the RemoveBackingStore above, and the cache
    // won't be over-sized.
    CreateCacheSpace((current_mem + new_mem) - max_mem);
  }
  DCHECK((BackingStoreManager::MemorySize() + new_mem) < max_mem);

  BackingStore* backing_store = host->AllocBackingStore(backing_store_size);
  if (new_mem > kSmallThreshold)
    large_cache->Put(host, backing_store);
  else
    small_cache->Put(host, backing_store);
  return backing_store;
}

}  // namespace

// BackingStoreManager ---------------------------------------------------------

// static
BackingStore* BackingStoreManager::GetBackingStore(
    RenderWidgetHost* host,
    const gfx::Size& desired_size) {
  BackingStore* backing_store = Lookup(host);
  if (backing_store) {
    // If we already have a backing store, then make sure it is the correct
    // size.
    if (backing_store->size() == desired_size)
      return backing_store;
    backing_store = NULL;
  }

  return backing_store;
}

// static
BackingStore* BackingStoreManager::PrepareBackingStore(
    RenderWidgetHost* host,
    const gfx::Size& backing_store_size,
    base::ProcessHandle process_handle,
    TransportDIB* bitmap,
    const gfx::Rect& bitmap_rect,
    bool* needs_full_paint) {
  BackingStore* backing_store = GetBackingStore(host, backing_store_size);
  if (!backing_store) {
    // We need to get Webkit to generate a new paint here, as we
    // don't have a previous snapshot.
    if (bitmap_rect.size() != backing_store_size ||
        bitmap_rect.x() != 0 || bitmap_rect.y() != 0) {
      DCHECK(needs_full_paint != NULL);
      *needs_full_paint = true;
    }
    backing_store = CreateBackingStore(host, backing_store_size);
  }

  DCHECK(backing_store != NULL);
  backing_store->PaintRect(process_handle, bitmap, bitmap_rect);
  return backing_store;
}

// static
BackingStore* BackingStoreManager::Lookup(RenderWidgetHost* host) {
  if (large_cache) {
    BackingStoreCache::iterator it = large_cache->Get(host);
    if (it != large_cache->end())
      return it->second;

    // This moves host to the front of the MRU.
    it = small_cache->Get(host);
    if (it != small_cache->end())
      return it->second;
  }
  return NULL;
}

// static
void BackingStoreManager::RemoveBackingStore(RenderWidgetHost* host) {
  if (!large_cache)
    return;

  BackingStoreCache* cache = large_cache;
  BackingStoreCache::iterator it = cache->Peek(host);
  if (it == cache->end()) {
    cache = small_cache;
    it = cache->Peek(host);
    if (it == cache->end())
      return;
  }
  cache->Erase(it);
}

// static
bool BackingStoreManager::ExpireBackingStoreForTest(RenderWidgetHost* host) {
  BackingStoreCache* cache = large_cache;

  BackingStoreCache::iterator it = cache->Peek(host);
  if (it == cache->end()) {
    cache = small_cache;
    it = cache->Peek(host);
    if (it == cache->end())
      return false;
  }
  ExpireBackingStoreAt(cache, it);
  return true;
}

// static
size_t BackingStoreManager::MemorySize() {
  if (!large_cache)
    return 0;

  size_t mem = 0;
  BackingStoreCache::iterator it;
  for (it = large_cache->begin(); it != large_cache->end(); ++it)
    mem += it->second->MemorySize();

  for (it = small_cache->begin(); it != small_cache->end(); ++it)
    mem += it->second->MemorySize();

  return mem;
}
