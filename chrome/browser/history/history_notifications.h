// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

// Details for HISTORY_URL_VISITED.
struct URLVisitedDetails : public HistoryDetails {
  PageTransition::Type transition;
  URLRow row;

  // A list of redirects leading up to the URL represented by this struct. If
  // we have the redirect chain A -> B -> C and this struct represents visiting
  // C, then redirects[0]=B and redirects[1]=A.  If there are no redirects,
  // this will be an empty vector.
  std::vector<GURL> redirects;
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
