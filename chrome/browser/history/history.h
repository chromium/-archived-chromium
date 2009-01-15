// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_HISTORY_H__
#define CHROME_BROWSER_HISTORY_HISTORY_H__

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/gfx/rect.h"
#include "base/lock.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/task.h"
#include "base/time.h"
#include "chrome/browser/cancelable_request.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/history/history_notifications.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/template_url.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/page_transition_types.h"
#include "chrome/common/ref_counted_util.h"

class BookmarkService;
struct DownloadCreateInfo;
class GURL;
class HistoryURLProvider;
struct HistoryURLProviderParams;
class InMemoryURLDatabase;
class MainPagesRequest;
enum NotificationType;
class PageUsageData;
class PageUsageRequest;
class Profile;
class SkBitmap;
struct ThumbnailScore;

namespace history {

class InMemoryHistoryBackend;
class HistoryBackend;
class HistoryDatabase;
class HistoryQueryTest;
class URLDatabase;

}  // namespace history


// HistoryDBTask can be used to process arbitrary work on the history backend
// thread. HistoryDBTask is scheduled using HistoryService::ScheduleDBTask.
// When HistoryBackend processes the task it invokes RunOnDBThread. Once the
// task completes and has not been canceled, DoneRunOnMainThread is invoked back
// on the main thread.
class HistoryDBTask : public base::RefCountedThreadSafe<HistoryDBTask> {
 public:
  virtual ~HistoryDBTask() {}

  // Invoked on the database thread. The return value indicates whether the
  // task is done. A return value of true signals the task is done and
  // RunOnDBThread should NOT be invoked again. A return value of false
  // indicates the task is not done, and should be run again after other
  // tasks are given a chance to be processed.
  virtual bool RunOnDBThread(history::HistoryBackend* backend,
                             history::HistoryDatabase* db) = 0;

  // Invoked on the main thread once RunOnDBThread has returned false. This is
  // only invoked if the request was not canceled and returned true from
  // RunOnDBThread.
  virtual void DoneRunOnMainThread() = 0;
};

// The history service records page titles, and visit times, as well as
// (eventually) information about autocomplete.
//
// This service is thread safe. Each request callback is invoked in the
// thread that made the request.
class HistoryService : public CancelableRequestProvider,
                       public NotificationObserver,
                       public base::RefCountedThreadSafe<HistoryService> {
 public:
  // Miscellaneous commonly-used types.
  typedef std::vector<GURL> RedirectList;
  typedef std::vector<PageUsageData*> PageUsageDataList;

  // ID (both star_id and group_id) of the bookmark bar.
  // This entry always exists.
  static const history::StarID kBookmarkBarID;

  // Must call Init after construction.
  explicit HistoryService(Profile* profile);
  // The empty constructor is provided only for testing.
  HistoryService();
  ~HistoryService();

  // Initializes the history service, returning true on success. On false, do
  // not call any other functions. The given directory will be used for storing
  // the history files. The BookmarkService is used when deleting URLs to
  // test if a URL is bookmarked; it may be NULL during testing.
  bool Init(const std::wstring& history_dir, BookmarkService* bookmark_service);

  // Did the backend finish loading the databases?
  bool backend_loaded() const { return backend_loaded_; }

  // Called on shutdown, this will tell the history backend to complete and
  // will release pointers to it. No other functions should be called once
  // cleanup has happened that may dispatch to the history thread (because it
  // will be NULL).
  //
  // In practice, this will be called by the service manager (BrowserProcess)
  // when it is being destroyed. Because that reference is being destroyed, it
  // should be impossible for anybody else to call the service, even if it is
  // still in memory (pending requests may be holding a reference to us).
  void Cleanup();

  // RenderProcessHost pointers are used to scope page IDs (see AddPage). These
  // objects must tell us when they are being destroyed so that we can clear
  // out any cached data associated with that scope.
  //
  // The given pointer will not be dereferenced, it is only used for
  // identification purposes, hence it is a void*.
  void NotifyRenderProcessHostDestruction(const void* host);

  // Returns the in-memory URL database. The returned pointer MAY BE NULL if
  // the in-memory database has not been loaded yet. This pointer is owned
  // by the history system. Callers should not store or cache this value.
  //
  // TODO(brettw) this should return the InMemoryHistoryBackend.
  history::URLDatabase* in_memory_database() const;

  // Navigation ----------------------------------------------------------------

  // Adds the given canonical URL to history with the current time as the visit
  // time. Referrer may be the empty string.
  //
  // The supplied render process host is used to scope the given page ID. Page
  // IDs are only unique inside a given render process, so we need that to
  // differentiate them. This pointer should not be dereferenced by the history
  // system. Since render view host pointers may be reused (if one gets deleted
  // and a new one created at the same address), WebContents should notify
  // us when they are being destroyed through NotifyWebContentsDestruction.
  //
  // The scope/ids can be NULL if there is no meaningful tracking information
  // that can be performed on the given URL. The 'page_id' should be the ID of
  // the current session history entry in the given process.
  //
  // 'redirects' is an array of redirect URLs leading to this page, with the
  // page itself as the last item (so when there is no redirect, it will have
  // one entry). If there are no redirects, this array may also be empty for
  // the convenience of callers.
  //
  // All "Add Page" functions will update the visited link database.
  void AddPage(const GURL& url,
               const void* id_scope,
               int32 page_id,
               const GURL& referrer,
               PageTransition::Type transition,
               const RedirectList& redirects);

  // For adding pages to history with a specific time. This is for testing
  // purposes. Call the previous one to use the current time.
  void AddPage(const GURL& url,
               base::Time time,
               const void* id_scope,
               int32 page_id,
               const GURL& referrer,
               PageTransition::Type transition,
               const RedirectList& redirects);

  // For adding pages to history where no tracking information can be done.
  void AddPage(const GURL& url) {
    AddPage(url, NULL, 0, GURL::EmptyGURL(), PageTransition::LINK,
            RedirectList());
  }

  // Sets the title for the given page. The page should be in history. If it
  // is not, this operation is ignored. This call will not update the full
  // text index. The last title set when the page is indexed will be the
  // title in the full text index.
  void SetPageTitle(const GURL& url, const std::wstring& title);

  // Indexing ------------------------------------------------------------------

  // Notifies history of the body text of the given recently-visited URL.
  // If the URL was not visited "recently enough," the history system may
  // discard it.
  void SetPageContents(const GURL& url, const std::wstring& contents);

  // Querying ------------------------------------------------------------------

  // Callback class that a client can implement to iterate over URLs. The
  // callbacks WILL BE CALLED ON THE BACKGROUND THREAD! Your implementation
  // should handle this appropriately.
  class URLEnumerator {
   public:
    virtual ~URLEnumerator() {}

    // Indicates that a URL is available. There will be exactly one call for
    // every URL in history.
    virtual void OnURL(const GURL& url) = 0;

    // Indicates we are done iterating over URLs. Once called, there will be no
    // more callbacks made. This call is guaranteed to occur, even if there are
    // no URLs. If all URLs were iterated, success will be true.
    virtual void OnComplete(bool success) = 0;
  };

  // Enumerate all URLs in history. The given iterator will be owned by the
  // caller, so the caller should ensure it exists until OnComplete is called.
  // You should not generally use this since it will be slow to slurp all URLs
  // in from the database. It is designed for rebuilding the visited link
  // database from history.
  void IterateURLs(URLEnumerator* iterator);

  // Returns the information about the requested URL. If the URL is found,
  // success will be true and the information will be in the URLRow parameter.
  // On success, the visits, if requested, will be sorted by date. If they have
  // not been requested, the pointer will be valid, but the vector will be
  // empty.
  //
  // If success is false, neither the row nor the vector will be valid.
  typedef Callback4<Handle,
                    bool,  // Success flag, when false, nothing else is valid.
                    const history::URLRow*,
                    history::VisitVector*>::Type
      QueryURLCallback;

  // Queries the basic information about the URL in the history database. If
  // the caller is interested in the visits (each time the URL is visited),
  // set |want_visits| to true. If these are not needed, the function will be
  // faster by setting this to false.
  Handle QueryURL(const GURL& url,
                  bool want_visits,
                  CancelableRequestConsumerBase* consumer,
                  QueryURLCallback* callback);

  // Provides the result of a query. See QueryResults in history_types.h.
  // The common use will be to use QueryResults.Swap to suck the contents of
  // the results out of the passed in parameter and take ownership of them.
  typedef Callback2<Handle, history::QueryResults*>::Type
      QueryHistoryCallback;

  // Queries all history with the given options (see QueryOptions in
  // history_types.h). If non-empty, the full-text database will be queried with
  // the given |text_query|. If empty, all results matching the given options
  // will be returned.
  //
  // This isn't totally hooked up yet, this will query the "new" full text
  // database (see SetPageContents) which won't generally be set yet.
  Handle QueryHistory(const std::wstring& text_query,
                      const history::QueryOptions& options,
                      CancelableRequestConsumerBase* consumer,
                      QueryHistoryCallback* callback);

  // Called when the results of QueryRedirectsFrom are available.
  // The given vector will contain a list of all redirects, not counting
  // the original page. If A redirects to B, the vector will contain only B,
  // and A will be in 'source_url'.
  //
  // If there is no such URL in the database or the most recent visit has no
  // redirect, the vector will be empty. If the history system failed for
  // some reason, success will additionally be false. If the given page
  // has redirected to multiple destinations, this will pick a random one.
  typedef Callback4<Handle,
                    GURL,  // from_url
                    bool,  // success
                    RedirectList*>::Type
      QueryRedirectsCallback;

  // Schedules a query for the most recent redirect coming out of the given
  // URL. See the RedirectQuerySource above, which is guaranteed to be called
  // if the request is not canceled.
  Handle QueryRedirectsFrom(const GURL& from_url,
                            CancelableRequestConsumerBase* consumer,
                            QueryRedirectsCallback* callback);

  typedef Callback4<Handle,
                    bool,        // Were we able to determine the # of visits?
                    int,         // Number of visits.
                    base::Time>::Type // Time of first visit. Only first bool is
                                 // true and int is > 0.
      GetVisitCountToHostCallback;

  // Requests the number of visits to all urls on the scheme/host/post
  // identified by url. This is only valid for http and https urls.
  Handle GetVisitCountToHost(const GURL& url,
                             CancelableRequestConsumerBase* consumer,
                             GetVisitCountToHostCallback* callback);

  // Thumbnails ----------------------------------------------------------------

  // Implemented by consumers to get thumbnail data. Called when a request for
  // the thumbnail data is complete. Once this callback is made, the request
  // will be completed and no other calls will be made for that handle.
  //
  // This function will be called even on error conditions or if there is no
  // thumbnail for that page. In these cases, the data pointer will be NULL.
  typedef Callback2<Handle, scoped_refptr<RefCountedBytes> >::Type
      ThumbnailDataCallback;

  // Sets the thumbnail for a given URL. The URL must be in the history
  // database or the request will be ignored.
  void SetPageThumbnail(const GURL& url,
                        const SkBitmap& thumbnail,
                        const ThumbnailScore& score);

  // Requests a page thumbnail. See ThumbnailDataCallback definition above.
  Handle GetPageThumbnail(const GURL& page_url,
                          CancelableRequestConsumerBase* consumer,
                          ThumbnailDataCallback* callback);

  // Favicon -------------------------------------------------------------------

  // Callback for GetFavIcon. If we have previously inquired about the favicon
  // for this URL, |know_favicon| will be true, and the rest of the fields will
  // be valid (otherwise they will be ignored).
  //
  // On |know_favicon| == true, |data| will either contain the PNG encoded
  // favicon data, or it will be NULL to indicate that the site does not have
  // a favicon (in other words, we know the site doesn't have a favicon, as
  // opposed to not knowing anything). |expired| will be set to true if we
  // refreshed the favicon "too long" ago and should be updated if the page
  // is visited again.
  typedef Callback5<Handle,                          // handle
                    bool,                            // know_favicon
                    scoped_refptr<RefCountedBytes>,  // data
                    bool,                            // expired
                    GURL>::Type                      // url of the favicon
      FavIconDataCallback;

  // Requests the favicon. FavIconConsumer is notified
  // when the bits have been fetched. The consumer is NOT deleted by the
  // HistoryService, and must be valid until the request is serviced.
  Handle GetFavIcon(const GURL& icon_url,
                    CancelableRequestConsumerBase* consumer,
                    FavIconDataCallback* callback);

  // Fetches the favicon at icon_url, sending the results to the given callback.
  // If the favicon has previously been set via SetFavIcon(), then the favicon
  // url for page_url and all redirects is set to icon_url. If the favicon has
  // not been set, the database is not updated.
  Handle UpdateFavIconMappingAndFetch(const GURL& page_url,
                                      const GURL& icon_url,
                                      CancelableRequestConsumerBase* consumer,
                                      FavIconDataCallback* callback);

  // Requests a favicon for a web page URL. FavIconConsumer is notified
  // when the bits have been fetched. The consumer is NOT deleted by the
  // HistoryService, and must be valid until the request is serviced.
  //
  // Note: this version is intended to be used to retrieve the favicon of a
  // page that has been browsed in the past. |expired| in the callback is
  // always false.
  Handle GetFavIconForURL(const GURL& page_url,
                          CancelableRequestConsumerBase* consumer,
                          FavIconDataCallback* callback);

  // Sets the favicon for a page.
  void SetFavIcon(const GURL& page_url,
                  const GURL& icon_url,
                  const std::vector<unsigned char>& image_data);

  // Marks the favicon for the page as being out of date.
  void SetFavIconOutOfDateForPage(const GURL& page_url);

  // Allows the importer to set many favicons for many pages at once. The pages
  // must exist, any favicon sets for unknown pages will be discarded. Existing
  // favicons will not be overwritten.
  void SetImportedFavicons(
      const std::vector<history::ImportedFavIconUsage>& favicon_usage);

  // Database management operations --------------------------------------------

  // Delete all the information related to a single url.
  void DeleteURL(const GURL& url);

  // Implemented by the caller of 'ExpireHistory(Since|Between)' below, and
  // is called when the history service has deleted the history.
  typedef Callback0::Type ExpireHistoryCallback;

  // Removes all visits in the selected time range (including the start time),
  // updating the URLs accordingly. This deletes the associated data, including
  // the full text index. This function also deletes the associated favicons,
  // if they are no longer referenced. |callback| runs when the expiration is
  // complete. You may use null Time values to do an unbounded delete in
  // either direction.
  void ExpireHistoryBetween(base::Time begin_time, base::Time end_time,
                            CancelableRequestConsumerBase* consumer,
                            ExpireHistoryCallback* callback);

  // Downloads -----------------------------------------------------------------

  // Implemented by the caller of 'CreateDownload' below, and is called when the
  // history service has created a new entry for a download in the history db.
  typedef Callback2<DownloadCreateInfo, int64>::Type DownloadCreateCallback;

  // Begins a history request to create a new persistent entry for a download.
  // 'info' contains all the download's creation state, and 'callback' runs
  // when the history service request is complete.
  Handle CreateDownload(const DownloadCreateInfo& info,
                        CancelableRequestConsumerBase* consumer,
                        DownloadCreateCallback* callback);

  // Implemented by the caller of 'QueryDownloads' below, and is called when the
  // history service has retrieved a list of all download state. The call
  typedef Callback1<std::vector<DownloadCreateInfo>*>::Type
      DownloadQueryCallback;

  // Begins a history request to retrieve the state of all downloads in the
  // history db. 'callback' runs when the history service request is complete,
  // at which point 'info' contains an array of DownloadCreateInfo, one per
  // download.
  Handle QueryDownloads(CancelableRequestConsumerBase* consumer,
                        DownloadQueryCallback* callback);

  // Called to update the history service about the current state of a download.
  // This is a 'fire and forget' query, so just pass the relevant state info to
  // the database with no need for a callback.
  void UpdateDownload(int64 received_bytes, int32 state, int64 db_handle);

  // Called to update the history service about the path of a download.
  // This is a 'fire and forget' query.
  void UpdateDownloadPath(const std::wstring& path, int64 db_handle);

  // Permanently remove a download from the history system. This is a 'fire and
  // forget' operation.
  void RemoveDownload(int64 db_handle);

  // Permanently removes all completed download from the history system within
  // the specified range. This function does not delete downloads that are in
  // progress or in the process of being cancelled. This is a 'fire and forget'
  // operation. You can pass is_null times to get unbounded time in either or
  // both directions.
  void RemoveDownloadsBetween(base::Time remove_begin, base::Time remove_end);

  // Implemented by the caller of 'SearchDownloads' below, and is called when
  // the history system has retrieved the search results.
  typedef Callback2<Handle, std::vector<int64>*>::Type DownloadSearchCallback;

  // Search for downloads that match the search text.
  Handle SearchDownloads(const std::wstring& search_text,
                         CancelableRequestConsumerBase* consumer,
                         DownloadSearchCallback* callback);

  // Visit Segments ------------------------------------------------------------

  typedef Callback2<Handle, std::vector<PageUsageData*>*>::Type
      SegmentQueryCallback;

  // Query usage data for all visit segments since the provided time.
  //
  // The request is performed asynchronously and can be cancelled by using the
  // returned handle.
  //
  // The vector provided to the callback and its contents is owned by the
  // history system. It will be deeply deleted after the callback is invoked.
  // If you want to preserve any PageUsageData instance, simply remove them
  // from the vector.
  //
  // The vector contains a list of PageUsageData. Each PageUsageData ID is set
  // to the segment ID. The URL and all the other information is set to the page
  // representing the segment.
  Handle QuerySegmentUsageSince(CancelableRequestConsumerBase* consumer,
                                const base::Time from_time,
                                SegmentQueryCallback* callback);

  // Set the presentation index for the segment identified by |segment_id|.
  void SetSegmentPresentationIndex(int64 segment_id, int index);

  // Keyword search terms -----------------------------------------------------

  // Sets the search terms for the specified url and keyword. url_id gives the
  // id of the url, keyword_id the id of the keyword and term the search term.
  void SetKeywordSearchTermsForURL(const GURL& url,
                                   TemplateURL::IDType keyword_id,
                                   const std::wstring& term);

  // Deletes all search terms for the specified keyword.
  void DeleteAllSearchTermsForKeyword(TemplateURL::IDType keyword_id);

  typedef Callback2<Handle, std::vector<history::KeywordSearchTermVisit>*>::Type
      GetMostRecentKeywordSearchTermsCallback;

  // Returns up to max_count of the most recent search terms starting with the
  // specified text. The matching is case insensitive. The results are ordered
  // in descending order up to |max_count| with the most recent search term
  // first.
  Handle GetMostRecentKeywordSearchTerms(
      TemplateURL::IDType keyword_id,
      const std::wstring& prefix,
      int max_count,
      CancelableRequestConsumerBase* consumer,
      GetMostRecentKeywordSearchTermsCallback* callback);

  // Bookmarks -----------------------------------------------------------------

  // Notification that a URL is no longer bookmarked.
  void URLsNoLongerBookmarked(const std::set<GURL>& urls);

  // Generic Stuff -------------------------------------------------------------

  typedef Callback0::Type HistoryDBTaskCallback;

  // Schedules a HistoryDBTask for running on the history backend thread. See
  // HistoryDBTask for details on what this does.
  Handle ScheduleDBTask(HistoryDBTask* task,
                        CancelableRequestConsumerBase* consumer);

  // Testing -------------------------------------------------------------------

  // Designed for unit tests, this passes the given task on to the history
  // backend to be called once the history backend has terminated. This allows
  // callers to know when the history thread is complete and the database files
  // can be deleted and the next test run. Otherwise, the history thread may
  // still be running, causing problems in subsequent tests.
  //
  // There can be only one closing task, so this will override any previously
  // set task. We will take ownership of the pointer and delete it when done.
  // The task will be run on the calling thread (this function is threadsafe).
  void SetOnBackendDestroyTask(Task* task);

  // Used for unit testing and potentially importing to get known information
  // into the database. This assumes the URL doesn't exist in the database
  //
  // Calling this function many times may be slow because each call will
  // dispatch to the history thread and will be a separate database
  // transaction. If this functionality is needed for importing many URLs, a
  // version that takes an array should probably be added.
  void AddPageWithDetails(const GURL& url,
                          const std::wstring& title,
                          int visit_count,
                          int typed_count,
                          base::Time last_visit,
                          bool hidden);

  // The same as AddPageWithDetails() but takes a vector.
  void AddPagesWithDetails(const std::vector<history::URLRow>& info);

 private:
  class BackendDelegate;
  friend class BackendDelegate;
  friend class history::HistoryBackend;
  friend class history::HistoryQueryTest;
  friend class HistoryOperation;
  friend class HistoryURLProvider;
  friend class HistoryURLProviderTest;
  template<typename Info, typename Callback> friend class DownloadRequest;
  friend class PageUsageRequest;
  friend class RedirectRequest;
  friend class FavIconRequest;
  friend class TestingProfile;

  // These are not currently used, hopefully we can do something in the future
  // to ensure that the most important things happen first.
  enum SchedulePriority {
    PRIORITY_UI,      // The highest priority (must respond to UI events).
    PRIORITY_NORMAL,  // Normal stuff like adding a page.
    PRIORITY_LOW,     // Low priority things like indexing or expiration.
  };

  // Implementation of NotificationObserver.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Called by the HistoryURLProvider class to schedule an autocomplete, it
  // will be called back on the internal history thread with the history
  // database so it can query. See history_autocomplete.cc for a diagram.
  void ScheduleAutocomplete(HistoryURLProvider* provider,
                            HistoryURLProviderParams* params);

  // Broadcasts the given notification. This is called by the backend so that
  // the notification will be broadcast on the main thread.
  //
  // The |details_deleted| pointer will be sent as the "details" for the
  // notification. The function takes ownership of the pointer and deletes it
  // when the notification is sent (it is coming from another thread, so must
  // be allocated on the heap).
  void BroadcastNotifications(NotificationType type,
                              history::HistoryDetails* details_deleted);

  // Notification from the backend that it has finished loading. Sends
  // notification (NOTIFY_HISTORY_LOADED) and sets backend_loaded_ to true.
  void OnDBLoaded();

  // Returns true if this looks like the type of URL we want to add to the
  // history. We filter out some URLs such as JavaScript.
  bool CanAddURL(const GURL& url) const;

  // Sets the in-memory URL database. This is called by the backend once the
  // database is loaded to make it available.
  void SetInMemoryBackend(history::InMemoryHistoryBackend* mem_backend);

  // Called by our BackendDelegate when the database version is too new to be
  // read properly.
  void NotifyTooNew();

  // Call to schedule a given task for running on the history thread with the
  // specified priority. The task will have ownership taken.
  void ScheduleTask(SchedulePriority priority, Task* task);

  // Schedule ------------------------------------------------------------------
  //
  // Functions for scheduling operations on the history thread that have a
  // handle and are cancelable. For fire-and-forget operations, see
  // ScheduleAndForget below.

  template<typename BackendFunc, class RequestType>
  Handle Schedule(SchedulePriority priority,
                  BackendFunc func,  // Function to call on the HistoryBackend.
                  CancelableRequestConsumerBase* consumer,
                  RequestType* request) {
    DCHECK(history_backend_) << "History service being called after cleanup";
    AddRequest(request, consumer);
    ScheduleTask(priority,
                 NewRunnableMethod(history_backend_.get(), func,
                                   scoped_refptr<RequestType>(request)));
    return request->handle();
  }

  template<typename BackendFunc, class RequestType, typename ArgA>
  Handle Schedule(SchedulePriority priority,
                  BackendFunc func,  // Function to call on the HistoryBackend.
                  CancelableRequestConsumerBase* consumer,
                  RequestType* request,
                  const ArgA& a) {
    DCHECK(history_backend_) << "History service being called after cleanup";
    AddRequest(request, consumer);
    ScheduleTask(priority,
                 NewRunnableMethod(history_backend_.get(), func,
                                   scoped_refptr<RequestType>(request),
                                   a));
    return request->handle();
  }

  template<typename BackendFunc,
           class RequestType,  // Descendant of CancelableRequstBase.
           typename ArgA,
           typename ArgB>
  Handle Schedule(SchedulePriority priority,
                  BackendFunc func,  // Function to call on the HistoryBackend.
                  CancelableRequestConsumerBase* consumer,
                  RequestType* request,
                  const ArgA& a,
                  const ArgB& b) {
    DCHECK(history_backend_) << "History service being called after cleanup";
    AddRequest(request, consumer);
    ScheduleTask(priority,
                 NewRunnableMethod(history_backend_.get(), func,
                                   scoped_refptr<RequestType>(request),
                                   a, b));
    return request->handle();
  }

  template<typename BackendFunc,
           class RequestType,  // Descendant of CancelableRequstBase.
           typename ArgA,
           typename ArgB,
           typename ArgC>
  Handle Schedule(SchedulePriority priority,
                  BackendFunc func,  // Function to call on the HistoryBackend.
                  CancelableRequestConsumerBase* consumer,
                  RequestType* request,
                  const ArgA& a,
                  const ArgB& b,
                  const ArgC& c) {
    DCHECK(history_backend_) << "History service being called after cleanup";
    AddRequest(request, consumer);
    ScheduleTask(priority,
                 NewRunnableMethod(history_backend_.get(), func,
                                   scoped_refptr<RequestType>(request),
                                   a, b, c));
    return request->handle();
  }

  // ScheduleAndForget ---------------------------------------------------------
  //
  // Functions for scheduling operations on the history thread that do not need
  // any callbacks and are not cancelable.

  template<typename BackendFunc>
  void ScheduleAndForget(SchedulePriority priority,
                         BackendFunc func) {  // Function to call on backend.
    DCHECK(history_backend_) << "History service being called after cleanup";
    ScheduleTask(priority, NewRunnableMethod(history_backend_.get(), func));
  }

  template<typename BackendFunc, typename ArgA>
  void ScheduleAndForget(SchedulePriority priority,
                         BackendFunc func,  // Function to call on backend.
                         const ArgA& a) {
    DCHECK(history_backend_) << "History service being called after cleanup";
    ScheduleTask(priority, NewRunnableMethod(history_backend_.get(), func, a));
  }

  template<typename BackendFunc, typename ArgA, typename ArgB>
  void ScheduleAndForget(SchedulePriority priority,
                         BackendFunc func,  // Function to call on backend.
                         const ArgA& a,
                         const ArgB& b) {
    DCHECK(history_backend_) << "History service being called after cleanup";
    ScheduleTask(priority, NewRunnableMethod(history_backend_.get(), func,
                                             a, b));
  }

  template<typename BackendFunc, typename ArgA, typename ArgB, typename ArgC>
  void ScheduleAndForget(SchedulePriority priority,
                         BackendFunc func,  // Function to call on backend.
                         const ArgA& a,
                         const ArgB& b,
                         const ArgC& c) {
    DCHECK(history_backend_) << "History service being called after cleanup";
    ScheduleTask(priority, NewRunnableMethod(history_backend_.get(), func,
                                             a, b, c));
  }

  template<typename BackendFunc,
           typename ArgA,
           typename ArgB,
           typename ArgC,
           typename ArgD>
  void ScheduleAndForget(SchedulePriority priority,
                         BackendFunc func,  // Function to call on backend.
                         const ArgA& a,
                         const ArgB& b,
                         const ArgC& c,
                         const ArgD& d) {
    DCHECK(history_backend_) << "History service being called after cleanup";
    ScheduleTask(priority, NewRunnableMethod(history_backend_.get(), func,
                                             a, b, c, d));
  }

  // Some void primitives require some internal processing in the main thread
  // when done. We use this internal consumer for this purpose.
  CancelableRequestConsumer internal_consumer_;

  // The thread used by the history service to run complicated operations
  ChromeThread* thread_;

  // This class has most of the implementation and runs on the 'thread_'.
  // You MUST communicate with this class ONLY through the thread_'s
  // message_loop().
  //
  // This pointer will be NULL once Cleanup() has been called, meaning no
  // more calls should be made to the history thread.
  scoped_refptr<history::HistoryBackend> history_backend_;

  // A cache of the user-typed URLs kept in memory that is used by the
  // autocomplete system. This will be NULL until the database has been created
  // on the background thread.
  scoped_ptr<history::InMemoryHistoryBackend> in_memory_backend_;

  // The profile, may be null when testing.
  Profile* profile_;

  // Has the backend finished loading? The backend is loaded once Init has
  // completed.
  bool backend_loaded_;

  DISALLOW_EVIL_CONSTRUCTORS(HistoryService);
};

#endif  // CHROME_BROWSER_HISTORY_HISTORY_H__
