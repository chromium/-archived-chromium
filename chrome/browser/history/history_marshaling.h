// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Data structures for communication between the history service on the main
// thread and the backend on the history thread.

#ifndef CHROME_BROWSER_HISTORY_HISTORY_MARSHALING_H__
#define CHROME_BROWSER_HISTORY_HISTORY_MARSHALING_H__

#include "base/scoped_vector.h"
#include "chrome/browser/cancelable_request.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/history/page_usage_data.h"

namespace history {

// Navigation -----------------------------------------------------------------

// Marshalling structure for AddPage.
class HistoryAddPageArgs : public base::RefCounted<HistoryAddPageArgs> {
 public:
  HistoryAddPageArgs(const GURL& arg_url,
                     base::Time arg_time,
                     const void* arg_id_scope,
                     int32 arg_page_id,
                     const GURL& arg_referrer,
                     const HistoryService::RedirectList& arg_redirects,
                     PageTransition::Type arg_transition,
                     bool arg_did_replace_entry)
      : url(arg_url),
        time(arg_time),
        id_scope(arg_id_scope),
        page_id(arg_page_id),
        referrer(arg_referrer),
        redirects(arg_redirects),
        transition(arg_transition),
        did_replace_entry(arg_did_replace_entry){
  }

  GURL url;
  base::Time time;

  const void* id_scope;
  int32 page_id;

  GURL referrer;
  HistoryService::RedirectList redirects;
  PageTransition::Type transition;
  bool did_replace_entry;

 private:
  DISALLOW_EVIL_CONSTRUCTORS(HistoryAddPageArgs);
};

// Querying -------------------------------------------------------------------

typedef CancelableRequest1<HistoryService::QueryURLCallback,
                           Tuple2<URLRow, VisitVector> >
    QueryURLRequest;

typedef CancelableRequest1<HistoryService::QueryHistoryCallback,
                           QueryResults>
    QueryHistoryRequest;

typedef CancelableRequest1<HistoryService::QueryRedirectsCallback,
                           HistoryService::RedirectList>
    QueryRedirectsRequest;

typedef CancelableRequest<HistoryService::GetVisitCountToHostCallback>
    GetVisitCountToHostRequest;

// Thumbnails -----------------------------------------------------------------

typedef CancelableRequest<HistoryService::ThumbnailDataCallback>
    GetPageThumbnailRequest;

// Favicons -------------------------------------------------------------------

typedef CancelableRequest<HistoryService::FavIconDataCallback>
    GetFavIconRequest;

// Downloads ------------------------------------------------------------------

typedef CancelableRequest1<HistoryService::DownloadQueryCallback,
                           std::vector<DownloadCreateInfo> >
    DownloadQueryRequest;

typedef CancelableRequest<HistoryService::DownloadCreateCallback>
    DownloadCreateRequest;

typedef CancelableRequest1<HistoryService::DownloadSearchCallback,
                          std::vector<int64> >
    DownloadSearchRequest;

// Deletion --------------------------------------------------------------------

typedef CancelableRequest<HistoryService::ExpireHistoryCallback>
    ExpireHistoryRequest;

// Segment usage --------------------------------------------------------------

typedef CancelableRequest1<HistoryService::SegmentQueryCallback,
                           ScopedVector<PageUsageData> >
    QuerySegmentUsageRequest;

// Keyword search terms -------------------------------------------------------

typedef
    CancelableRequest1<HistoryService::GetMostRecentKeywordSearchTermsCallback,
                       std::vector<KeywordSearchTermVisit> >
    GetMostRecentKeywordSearchTermsRequest;

// Generic operations ---------------------------------------------------------

// The argument here is an input value, which is the task to run on the
// background thread. The callback is used to execute the portion of the task
// that executes on the main thread.
typedef CancelableRequest1<HistoryService::HistoryDBTaskCallback,
                           scoped_refptr<HistoryDBTask> >
    HistoryDBTaskRequest;

}  // namespace history

#endif  // CHROME_BROWSER_HISTORY_HISTORY_MARSHALING_H__
