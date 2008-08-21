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

// Structs that hold data used in broadcasting notifications.

#ifndef CHROME_BROWSER_HISTORY_HISTORY_NOTIFICATIONS_H__
#define CHROME_BROWSER_HISTORY_HISTORY_NOTIFICATIONS_H__

#include <set>
#include <vector>

#include "googleurl/src/gurl.h"
#include "chrome/browser/history/history_types.h"

namespace history {

// Base class for history notifications. This needs only a virtual destructor
// so that the history service's broadcaster can delete it when the request
// is complete.
struct HistoryDetails {
 public:
  virtual ~HistoryDetails() {}
};

// Details for NOTIFY_HISTORY_URL_VISITED.
struct URLVisitedDetails : public HistoryDetails {
  URLRow row;
};

// Details for NOTIFY_HISTORY_TYPED_URLS_MODIFIED.
struct URLsModifiedDetails : public HistoryDetails {
  // Lists the information for each of the URLs affected.
  std::vector<URLRow> changed_urls;
};

// Details for NOTIFY_HISTORY_URLS_DELETED.
struct URLsDeletedDetails : public HistoryDetails {
  // Set when all history was deleted. False means just a subset was deleted.
  bool all_history;

  // The list of unique URLs affected. This is valid only when a subset of
  // history is deleted. When all of it is deleted, this will be empty, since
  // we do not bother to list all URLs.
  std::set<GURL> urls;
};

// Details for NOTIFY_URLS_STARRED.
struct URLsStarredDetails : public HistoryDetails {
  URLsStarredDetails(bool being_starred) : starred(being_starred) {}

  // The new starred state of the list of URLs. True when they are being
  // starred, false when they are being unstarred.
  bool starred;

  // The list of URLs that are changing.
  std::set<GURL> changed_urls;
};

// Details for NOTIFY_FAVICON_CHANGED.
struct FavIconChangeDetails : public HistoryDetails {
  std::set<GURL> urls;
};

}  // namespace history

#endif  // CHROME_BROWSER_HISTORY_HISTORY_NOTIFICATIONS_H__
