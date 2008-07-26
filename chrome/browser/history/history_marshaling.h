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

// Data structures for communication between the history service on the main
// thread and the backend on the history thread.

#ifndef CHROME_BROWSER_HISTORY_HISTORY_MARSHALING_H__
#define CHROME_BROWSER_HISTORY_HISTORY_MARSHALING_H__

#include "chrome/browser/cancelable_request.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/history/page_usage_data.h"
#include "chrome/common/scoped_vector.h"

namespace history {

// Navigation -----------------------------------------------------------------

// Marshalling structure for AddPage.
class HistoryAddPageArgs : public base::RefCounted<HistoryAddPageArgs> {
 public:
  HistoryAddPageArgs(const GURL& arg_url,
                     Time arg_time,
                     const void* arg_id_scope,
                     int32 arg_page_id,
                     const GURL& arg_referrer,
                     const HistoryService::RedirectList& arg_redirects,
                     PageTransition::Type arg_transition)
      : url(arg_url),
        time(arg_time),
        id_scope(arg_id_scope),
        page_id(arg_page_id),
        referrer(arg_referrer),
        redirects(arg_redirects),
        transition(arg_transition) {
  }

  GURL url;
  Time time;

  const void* id_scope;
  int32 page_id;

  GURL referrer;
  HistoryService::RedirectList redirects;
  PageTransition::Type transition;

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

// Starring -------------------------------------------------------------------

typedef CancelableRequest1<HistoryService::GetStarredEntriesCallback,
                           std::vector<history::StarredEntry> >
    GetStarredEntriesRequest;

typedef CancelableRequest1<HistoryService::GetMostRecentStarredEntriesCallback,
                           std::vector<history::StarredEntry> >
    GetMostRecentStarredEntriesRequest;

typedef CancelableRequest<HistoryService::CreateStarredEntryCallback>
    CreateStarredEntryRequest;

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
