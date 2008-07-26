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
//
// Class for finding and caching Windows explorer icons. The IconManager
// lives on the UI thread but performs icon extraction work on the file thread
// to avoid blocking the UI thread with potentially expensive COM and disk
// operations.
//
// Terminology
//
// Windows files have icons associated with them that can be of two types:
//   1. "Per class": the icon used for this file is used for all files with the
//      same file extension or class. Examples are PDF or MP3 files, which use
//      the same icon for all files of that type.
//   2. "Per instance": the icon used for this file is embedded in the file
//      itself and is unique. Executable files are typically "per instance".
//
// Files that end in the following extensions are considered "per instance":
//   .exe
//   .dll
//   .ico
// The IconManager will do explicit icon loads on the full path of these files
// and cache the results per file. All other file types will be looked up by
// file extension and the results will be cached per extension. That way, all
// .mp3 files will share one icon, but all .exe files will have their own icon.
//
// The IconManager can be queried in two ways:
//   1. A quick, synchronous check of its caches which does not touch the disk:
//      IconManager::LookupIcon()
//   2. An asynchronous icon load from a file on the file thread:
//      IconManager::LoadIcon()
//
// When using the second (asychronous) method, callers must supply a callback
// which will be run once the icon has been extracted. The icon manager will
// cache the results of the icon extraction so that subsequent lookups will be
// fast.
//
// Icon bitmaps returned should be treated as const since they may be referenced
// by other clients. Make a copy of the icon if you need to modify it.

#ifndef CHROME_BROWSER_ICON_MANAGER_H__
#define CHROME_BROWSER_ICON_MANAGER_H__

#include <hash_map>
#include <map>
#include <set>
#include <string>

#include "chrome/browser/icon_loader.h"

#include "chrome/browser/cancelable_request.h"

class SkBitmap;

class IconManager : public IconLoader::Delegate,
                    public CancelableRequestProvider {
public:
  IconManager();
  ~IconManager();

  // Synchronous call to examine the internal caches for the icon. Returns the
  // icon if we have already loaded it, NULL if we don't have it and must load
  // it via 'LoadIcon'. The returned bitmap is owned by the IconManager and must
  // not be free'd by the caller. If the caller needs to modify the icon, it
  // must make a copy and modify the copy.
  SkBitmap* LookupIcon(const std::wstring& file_name,
                       IconLoader::IconSize size);

  // Asynchronous call to lookup and return the icon associated with file. The
  // work is done on the file thread, with the callbacks running on the UI
  // thread. The return value is the 'request_id' that will be passed to the
  // client in the callback.
  typedef CancelableRequestProvider::Handle Handle;
  typedef Callback2<Handle, SkBitmap*>::Type IconRequestCallback;

  Handle LoadIcon(const std::wstring& file_name,
                  IconLoader::IconSize size,
                  CancelableRequestConsumerBase* consumer,
                  IconRequestCallback* callback);

  // IconLoader::Delegate interface.
  virtual bool OnSkBitmapLoaded(IconLoader* source, SkBitmap* result);
  virtual bool OnHICONLoaded(IconLoader* source,
                             HICON small_icon,
                             HICON large_icon);

private:
  struct CacheKey {
    CacheKey(std::wstring file_name, IconLoader::IconSize size);

    // Used as a key in the map below, so we need this comparator.
    bool operator<(const CacheKey &other) const;

    std::wstring file_name;
    IconLoader::IconSize size;
  };

  typedef std::map<CacheKey, SkBitmap*> IconMap;
  IconMap icon_cache_;

  typedef CancelableRequest<IconRequestCallback> IconRequest;
  typedef struct {
    scoped_refptr<IconRequest> request;
    std::wstring file_name;
    IconLoader::IconSize size;
  } ClientRequest;

  // Asynchronous requests that have not yet been completed.
  typedef stdext::hash_map<IconLoader*, ClientRequest> ClientRequests;
  ClientRequests requests_;

  DISALLOW_EVIL_CONSTRUCTORS(IconManager);
};

#endif  // #ifndef CHROME_BROWSER_ICON_MANAGER_H__