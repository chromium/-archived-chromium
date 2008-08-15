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

#include "base/path_service.h"

#ifdef OS_WIN
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#endif

#include "base/hash_tables.h"
#include "base/file_util.h"
#include "base/lock.h"
#include "base/logging.h"
#include "base/singleton.h"
#include "base/string_util.h"

namespace base {
  bool PathProvider(int key, std::wstring* result);
#if defined(OS_WIN)
  bool PathProviderWin(int key, std::wstring* result);
#elif defined(OS_MACOSX)
  bool PathProviderMac(int key, std::wstring* result);
#elif defined(OS_LINUX)
  bool PathProviderLinux(int key, std::wstring* result);
#endif
}

namespace {

typedef base::hash_map<int, std::wstring> PathMap;
typedef base::hash_set<int> PathSet;

// We keep a linked list of providers.  In a debug build we ensure that no two
// providers claim overlapping keys.
struct Provider {
  PathService::ProviderFunc func;
  struct Provider* next;
#ifndef NDEBUG
  int key_start;
  int key_end;
#endif
};

static Provider base_provider = {
  base::PathProvider,
  NULL,
#ifndef NDEBUG
  base::PATH_START,
  base::PATH_END
#endif
};

#ifdef OS_WIN
static Provider base_provider_win = {
  base::PathProviderWin,
  &base_provider,
#ifndef NDEBUG
  base::PATH_WIN_START,
  base::PATH_WIN_END
#endif
};
#endif

#ifdef OS_MACOSX
  static Provider base_provider_mac = {
  base::PathProviderMac,
  &base_provider,
#ifndef NDEBUG
  base::PATH_MAC_START,
  base::PATH_MAC_END
#endif
  };
#endif

#if defined(OS_LINUX)
static Provider base_provider_linux = {
  base::PathProviderLinux,
  &base_provider,
#ifndef NDEBUG
  base::PATH_LINUX_START,
  base::PATH_LINUX_END
#endif
  };
#endif


struct PathData {
  Lock      lock;
  PathMap   cache;      // Track mappings from path key to path value.
  PathSet   overrides;  // Track whether a path has been overridden.
  Provider* providers;  // Linked list of path service providers.

  PathData() {
#if defined(OS_WIN)
    providers = &base_provider_win;
#elif defined(OS_MACOSX)
    providers = &base_provider_mac;
#elif defined(OS_LINUX)
    providers = &base_provider_linux;
#endif
  }
};
  
static PathData* GetPathData() {
  return Singleton<PathData>::get();
}

}  // namespace


// static
bool PathService::GetFromCache(int key, std::wstring* result) {
  PathData* path_data = GetPathData();
  AutoLock scoped_lock(path_data->lock);
  
  // check for a cached version
  PathMap::const_iterator it = path_data->cache.find(key);
  if (it != path_data->cache.end()) {
    *result = it->second;
    return true;
  }
  return false;
}

// static
void PathService::AddToCache(int key, const std::wstring& path) {
  PathData* path_data = GetPathData();
  AutoLock scoped_lock(path_data->lock);
  // Save the computed path in our cache.
  path_data->cache[key] = path;
}

// TODO(brettw): this function does not handle long paths (filename > MAX_PATH)
// characters). This isn't supported very well by Windows right now, so it is
// moot, but we should keep this in mind for the future.
// static
bool PathService::Get(int key, std::wstring* result) {
  PathData* path_data = GetPathData();
  DCHECK(path_data);
  DCHECK(result);
  DCHECK(key >= base::DIR_CURRENT);

  // special case the current directory because it can never be cached
  if (key == base::DIR_CURRENT)
    return file_util::GetCurrentDirectory(result);

  if (GetFromCache(key, result))
    return true;
  
  std::wstring path;

  // search providers for the requested path
  // NOTE: it should be safe to iterate here without the lock
  // since RegisterProvider always prepends.
  Provider* provider = path_data->providers;
  while (provider) {
    if (provider->func(key, &path))
      break;
    DCHECK(path.empty()) << "provider should not have modified path";
    provider = provider->next;
  }

  if (path.empty())
    return false;

  AddToCache(key, path);
  
  result->swap(path);
  return true;
}

bool PathService::IsOverridden(int key) {
  PathData* path_data = GetPathData();
  DCHECK(path_data);

  AutoLock scoped_lock(path_data->lock);
  return path_data->overrides.find(key) != path_data->overrides.end();
}

bool PathService::Override(int key, const std::wstring& path) {
  PathData* path_data = GetPathData();
  DCHECK(path_data);
  DCHECK(key > base::DIR_CURRENT) << "invalid path key";

  std::wstring file_path = path;
  if (!file_util::AbsolutePath(&file_path))
    return false;

  // make sure the directory exists:
  if (!file_util::PathExists(file_path) &&
      // TODO(darin): what if this path is not that of a directory?
      !file_util::CreateDirectory(file_path))
    return false;

  file_util::TrimTrailingSeparator(&file_path);

  AutoLock scoped_lock(path_data->lock);
  path_data->cache[key] = file_path;
  path_data->overrides.insert(key);
  return true;
}

bool PathService::SetCurrentDirectory(const std::wstring& current_directory) {
  return file_util::SetCurrentDirectory(current_directory);
}

void PathService::RegisterProvider(ProviderFunc func, int key_start,
                                   int key_end) {
  PathData* path_data = GetPathData();
  DCHECK(path_data);
  DCHECK(key_end > key_start);

  AutoLock scoped_lock(path_data->lock);

  Provider* p;

#ifndef NDEBUG
  p = path_data->providers;
  while (p) {
    DCHECK(key_start >= p->key_end || key_end <= p->key_start) <<
      "path provider collision";
    p = p->next;
  }
#endif

  p = new Provider;
  p->func = func;
  p->next = path_data->providers;
#ifndef NDEBUG
  p->key_start = key_start;
  p->key_end = key_end;
#endif
  path_data->providers = p;
}
