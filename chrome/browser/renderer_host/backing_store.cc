// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/backing_store.h"

class RenderWidgetHost;

namespace {

typedef OwningMRUCache<RenderWidgetHost*, BackingStore*> BackingStoreCache;
static BackingStoreCache* cache = NULL;

// Returns the size of the backing store cache.
// TODO(iyengar) Make this dynamic, i.e. based on the available resources
// on the machine.
static int GetBackingStoreCacheSize() {
  const int kMaxSize = 5;
  return kMaxSize;
}

// Creates the backing store for the host based on the dimensions passed in.
// Removes the existing backing store if there is one.
BackingStore* CreateBackingStore(RenderWidgetHost* host,
                                 const gfx::Rect& backing_store_rect) {
  BackingStoreManager::RemoveBackingStore(host);

  BackingStore* backing_store = new BackingStore(backing_store_rect.size());
  int backing_store_cache_size = GetBackingStoreCacheSize();
  if (backing_store_cache_size > 0) {
    if (!cache)
      cache = new BackingStoreCache(backing_store_cache_size);
    cache->Put(host, backing_store);
  }
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
    const gfx::Rect& backing_store_rect,
    base::ProcessHandle process_handle,
    TransportDIB* bitmap,
    const gfx::Rect& bitmap_rect,
    bool* needs_full_paint) {
  BackingStore* backing_store = GetBackingStore(host,
                                                backing_store_rect.size());
  if (!backing_store) {
    // We need to get Webkit to generate a new paint here, as we
    // don't have a previous snapshot.
    if (bitmap_rect != backing_store_rect) {
      DCHECK(needs_full_paint != NULL);
      *needs_full_paint = true;
    }
    backing_store = CreateBackingStore(host, backing_store_rect);
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
