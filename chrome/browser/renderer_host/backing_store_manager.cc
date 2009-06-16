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

typedef OwningMRUCache<RenderWidgetHost*, BackingStore*> BackingStoreCache;
static BackingStoreCache* cache = NULL;

// Returns the size of the backing store cache.
static size_t GetBackingStoreCacheSize() {
  // This uses a similar approach to GetMaxRendererProcessCount.  The goal
  // is to reduce memory pressure and swapping on low-resource machines.
  static const size_t kMaxDibCountByRamTier[] = {
    2,  // less than 256MB
    3,  // 256MB
    4,  // 512MB
    5   // 768MB and above
  };

  static size_t max_size = kMaxDibCountByRamTier[
          std::min(base::SysInfo::AmountOfPhysicalMemoryMB() / 256,
                   static_cast<int>(arraysize(kMaxDibCountByRamTier)) - 1)];
  return max_size;
}

// Expires the given backing store from the cache.
void ExpireBackingStoreAt(BackingStoreCache::iterator backing_store) {
  RenderWidgetHost* rwh = backing_store->second->render_widget_host();
  if (rwh->painting_observer()) {
    rwh->painting_observer()->WidgetWillDestroyBackingStore(
        backing_store->first,
        backing_store->second);
  }
  cache->Erase(backing_store);
}

// Creates the backing store for the host based on the dimensions passed in.
// Removes the existing backing store if there is one.
BackingStore* CreateBackingStore(RenderWidgetHost* host,
                                 const gfx::Size& backing_store_size) {
  // Remove any existing backing store in case we're replacing it.
  BackingStoreManager::RemoveBackingStore(host);

  size_t max_cache_size = GetBackingStoreCacheSize();
  if (max_cache_size > 0 && !cache)
    cache = new BackingStoreCache(BackingStoreCache::NO_AUTO_EVICT);

  if (cache->size() >= max_cache_size) {
    // Need to remove an old backing store to make room for the new one. We
    // don't want to do this when the backing store is being replace by a new
    // one for the same tab, but this case won't get called then: we'll have
    // removed the onld one in the RemoveBackingStore above, and the cache
    // won't be over-sized.
    //
    // Crazy C++ alert: rbegin.base() is a forward iterator pointing to end(),
    // so we need to do -- to move one back to the actual last item.
    ExpireBackingStoreAt(--cache->rbegin().base());
  }

  BackingStore* backing_store = host->AllocBackingStore(backing_store_size);
  if (max_cache_size > 0)
    cache->Put(host, backing_store);
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
  if (cache) {
    BackingStoreCache::iterator it = cache->Peek(host);
    if (it != cache->end())
      return it->second;
  }
  return NULL;
}

// static
void BackingStoreManager::RemoveBackingStore(RenderWidgetHost* host) {
  if (!cache)
    return;

  BackingStoreCache::iterator it = cache->Peek(host);
  if (it == cache->end())
    return;
  cache->Erase(it);

  if (cache->empty()) {
    delete cache;
    cache = NULL;
  }
}

// static
bool BackingStoreManager::ExpireBackingStoreForTest(RenderWidgetHost* host) {
  BackingStoreCache::iterator it = cache->Peek(host);
  if (it == cache->end())
    return false;
  ExpireBackingStoreAt(it);
  return true;
}
