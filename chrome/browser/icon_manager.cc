// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

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
    return false;
  }

  ClientRequest client_request = rit->second;
  if (client_request.request->canceled()) {
    requests_.erase(rit);
    return true;
  }

  CacheKey key(client_request.file_name, client_request.size);
  IconMap::iterator it = icon_cache_.find(key);
  if (it != icon_cache_.end()) {
    it->second->swap(*result);
    delete result;
  } else {
    icon_cache_[key] = result;
  }

  // Inform our client that the request has completed.
  IconRequest* icon_request = client_request.request;
  icon_request->ForwardResult(IconRequest::TupleType(icon_request->handle(),
                                                     result));
  requests_.erase(rit);

  return true;
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

