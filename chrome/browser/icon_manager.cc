// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/icon_manager.h"

#include "base/file_util.h"
#include "base/scoped_ptr.h"
#include "chrome/common/stl_util-inl.h"
#include "SkBitmap.h"
#include "SkCanvas.h"

IconManager::IconManager() {
}

IconManager::~IconManager() {
  STLDeleteValues(&icon_cache_);
}

SkBitmap* IconManager::LookupIcon(const std::wstring& file_name,
                                  IconLoader::IconSize size) {
  std::wstring path = file_name;
  std::wstring extension = file_util::GetFileExtensionFromPath(path);
  if (extension != L"exe" && extension != L"dll" && extension != L"ico")
    path = L'.' + extension;

  IconMap::iterator it = icon_cache_.find(CacheKey(path, size));
  if (it != icon_cache_.end())
    return it->second;

  return NULL;
}

IconManager::Handle IconManager::LoadIcon(
    const std::wstring& file_name,
    IconLoader::IconSize size,
    CancelableRequestConsumerBase* consumer,
    IconRequestCallback* callback) {
  std::wstring path = file_name;
  std::wstring extension = file_util::GetFileExtensionFromPath(path);
  if (extension != L"exe" && extension != L"dll" && extension != L"ico")
    path = L'.' + extension;

  IconRequest* request = new IconRequest(callback);
  AddRequest(request, consumer);

  IconLoader* loader =
      IconLoader::CreateIconLoaderForFileResource(path, size, this);
  loader->Start();
  ClientRequest client_request = { request, path, size };
  requests_[loader] = client_request;
  return request->handle();
}

// IconLoader::Delegate implementation -----------------------------------------

bool IconManager::OnSkBitmapLoaded(IconLoader* source, SkBitmap* result) {
  scoped_ptr<IconLoader> scoped_source(source);

  // Look up our client state.
  ClientRequests::iterator rit = requests_.find(source);
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
  CacheKey key(client_request.file_name, client_request.size);
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

bool IconManager::OnHICONLoaded(IconLoader* source,
                                HICON small_icon,
                                HICON large_icon) {
  NOTREACHED();
  return false;
}

IconManager::CacheKey::CacheKey(std::wstring file_name,
                                IconLoader::IconSize size)
    : file_name(file_name),
      size(size) {
}

bool IconManager::CacheKey::operator<(const CacheKey &other) const {
  if (file_name != other.file_name)
    return file_name < other.file_name;
  return size < other.size;
}
