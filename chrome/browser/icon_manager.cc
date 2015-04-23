// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/icon_manager.h"

#include "base/file_util.h"
#include "base/scoped_ptr.h"
#include "base/stl_util-inl.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"

IconManager::IconManager() {
}

IconManager::~IconManager() {
  STLDeleteValues(&icon_cache_);
}

SkBitmap* IconManager::LookupIcon(const FilePath& file_name,
                                  IconLoader::IconSize size) {
  IconGroupID group = GetGroupIDFromFilepath(file_name);
  IconMap::iterator it = icon_cache_.find(CacheKey(group, size));
  if (it != icon_cache_.end())
    return it->second;

  return NULL;
}

IconManager::Handle IconManager::LoadIcon(
    const FilePath& file_name,
    IconLoader::IconSize size,
    CancelableRequestConsumerBase* consumer,
    IconRequestCallback* callback) {
  IconGroupID group = GetGroupIDFromFilepath(file_name);
  IconRequest* request = new IconRequest(callback);
  AddRequest(request, consumer);

  IconLoader* loader = new IconLoader(group, size, this);
  loader->AddRef();
  loader->Start();
  ClientRequest client_request = { request, group, size };
  requests_[loader] = client_request;
  return request->handle();
}

// IconLoader::Delegate implementation -----------------------------------------

bool IconManager::OnBitmapLoaded(IconLoader* source, SkBitmap* result) {
  ClientRequests::iterator rit = requests_.find(source);
  // Balances the AddRef() in LoadIcon().
  source->Release();

  // Look up our client state.
  if (rit == requests_.end()) {
    NOTREACHED();
    return false;  // Return false to indicate result should be deleted.
  }

  ClientRequest client_request = rit->second;
  if (client_request.request->canceled()) {
    requests_.erase(rit);
    return false;  // Return false to indicate result should be deleted.
  }

  // Cache the bitmap. Watch out: |result| or the cached bitmap may be NULL to
  // indicate a current or past failure.
  CacheKey key(client_request.group, client_request.size);
  IconMap::iterator it = icon_cache_.find(key);
  if (it != icon_cache_.end() && result && it->second) {
    it->second->swap(*result);
    delete result;
    result = it->second;
  } else {
    icon_cache_[key] = result;
  }

  // Inform our client that the request has completed.
  IconRequest* icon_request = client_request.request;
  icon_request->ForwardResult(IconRequest::TupleType(icon_request->handle(),
                                                     result));
  requests_.erase(rit);

  return true;  // Indicates we took ownership of result.
}

IconManager::CacheKey::CacheKey(const IconGroupID& group,
                                IconLoader::IconSize size)
    : group(group),
      size(size) {
}

bool IconManager::CacheKey::operator<(const CacheKey &other) const {
  if (group != other.group)
    return group < other.group;
  return size < other.size;
}
