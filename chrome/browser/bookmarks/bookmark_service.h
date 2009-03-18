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

#ifndef CHROME_BROWSER_BOOKMARKS_BOOKMARK_SERVICE_H_
#define CHROME_BROWSER_BOOKMARKS_BOOKMARK_SERVICE_H_

#include <vector>

class GURL;

// BookmarkService provides a thread safe view of bookmarks. It is used by
// HistoryBackend when it needs to determine the set of bookmarked URLs
// or if a URL is bookmarked.
//
// BookmarkService is owned by Profile and deleted when the Profile is deleted.
class BookmarkService {
 public:
  // Returns true if the specified URL is bookmarked.
  //
  // If not on the main thread you *must* invoke BlockTillLoaded first.
  virtual bool IsBookmarked(const GURL& url) = 0;

  // Returns, by reference in urls, the set of bookmarked urls. This returns
  // the unique set of URLs. For example, if two bookmarks reference the same
  // URL only one entry is added.
  //
  // If not on the main thread you *must* invoke BlockTillLoaded first.
  virtual void GetBookmarks(std::vector<GURL>* urls) = 0;

  // Blocks until loaded. This is intended for usage on a thread other than
  // the main thread.
  virtual void BlockTillLoaded() = 0;
};

#endif  // CHROME_BROWSER_BOOKMARKS_BOOKMARK_SERVICE_H_
