// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_HISTORY_BACKEND_H__
#define CHROME_BROWSER_HISTORY_HISTORY_BACKEND_H__

#include <utility>

#include "base/gfx/rect.h"
#include "base/lock.h"
#include "base/scoped_ptr.h"
#include "base/task.h"
#include "chrome/browser/history/archived_database.h"
#include "chrome/browser/history/download_types.h"
#include "chrome/browser/history/expire_history_backend.h"
#include "chrome/browser/history/history_database.h"
#include "chrome/browser/history/history_marshaling.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/history/page_usage_data.h"
#include "chrome/browser/history/text_database_manager.h"
#include "chrome/browser/history/thumbnail_database.h"
#include "chrome/browser/history/visit_tracker.h"
#include "chrome/common/mru_cache.h"
#include "chrome/common/scoped_vector.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

class BookmarkService;
class TestingProfile;
struct ThumbnailScore;

namespace history {

class CommitLaterTask;
class HistoryPublisher;

// *See the .cc file for more information on the design.*
//
// Internal history implementation which does most of the work of the history
// system. This runs on a background thread (to not block the browser when we
// do expensive operations) and is NOT threadsafe, so it must only be called
// from message handlers on the background thread. Invoking on another thread
// requires threadsafe refcounting.
//
// Most functions here are just the implementations of the corresponding
// functions in the history service. These functions are not documented
// here, see the history service for behavior.
class HistoryBackend : public base::RefCountedThreadSafe<HistoryBackend>,
                       public BroadcastNotificationDelegate {
 public:
  // Interface implemented by the owner of the HistoryBackend object. Normally,
  // the history service implements this to send stuff back to the main thread.
  // The unit tests can provide a different implementation if they don't have
  // a history service object.
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Called when the database is from a future version of the product and can
    // not be used.
    virtual void NotifyTooNew() = 0;

    // Sets the in-memory history backend. The in-memory backend is created by
    // the main backend. For non-unit tests, this happens on the background
    // thread. It is to be used on the main thread, so this would transfer
    // it to the history service. Unit tests can override this behavior.
    //
    // This function is NOT guaranteed to be called. If there is an error,
    // there may be no in-memory database.
    //
    // Ownership of the backend pointer is transferred to this function.
    virtual void SetInMemoryBackend(InMemoryHistoryBackend* backend) = 0;

    // Broadcasts the specified notification to the notification service.
    // This is implemented here because notifications must only be sent from
    // the main thread.
    //
    // Ownership of the HistoryDetails is transferred to this function.
    virtual void BroadcastNotifications(NotificationType type,
                                        HistoryDetails* details) = 0;

    // Invoked when the backend has finished loading the db.
    virtual void DBLoaded() = 0;
  };

  // Init must be called to complete object creation. This object can be
  // constructed on any thread, but all other functions including Init() must
  // be called on the history thread.
  //
  // |history_dir| is the directory where the history files will be placed.
  // See the definition of BroadcastNotificationsCallback above. This function
  // takes ownership of the callback pointer.
  //
  // |bookmark_service| is used to determine bookmarked URLs when deleting and
  // may be NULL.
  //
  // This constructor is fast and does no I/O, so can be called at any time.
  HistoryBackend(const std::wstring& history_dir,
                 Delegate* delegate,
                 BookmarkService* bookmark_service);

  ~HistoryBackend();

  // Must be called after creation but before any objects are created. If this
  // fails, all other functions will fail as well. (Since this runs on another
  // thread, we don't bother returning failure.)
  void Init();

  // Notification that the history system is shutting down. This will break
  // the refs owned by the delegate and any pending transaction so it will
  // actually be deleted.
  void Closing();

  // See NotifyRenderProcessHostDestruction.
  void NotifyRenderProcessHostDestruction(const void* host);

  // Navigation ----------------------------------------------------------------

  void AddPage(scoped_refptr<HistoryAddPageArgs> request);
  void SetPageTitle(const GURL& url, const std::wstring& title);
  void AddPageWithDetails(const URLRow& info);

  // Indexing ------------------------------------------------------------------

  void SetPageContents(const GURL& url, const std::wstring& contents);

  // Querying ------------------------------------------------------------------

  // ScheduleAutocomplete() never frees |provider| (which is globally live).
  // It passes |params| on to the autocomplete system which will eventually
  // free it.
  void ScheduleAutocomplete(HistoryURLProvider* provider,
                            HistoryURLProviderParams* params);

  void IterateURLs(HistoryService::URLEnumerator* enumerator);
  void QueryURL(scoped_refptr<QueryURLRequest> request,
                const GURL& url,
                bool want_visits);
  void QueryHistory(scoped_refptr<QueryHistoryRequest> request,
                    const std::wstring& text_query,
                    const QueryOptions& options);
  void QueryRedirectsFrom(scoped_refptr<QueryRedirectsRequest> request,
                          const GURL& url);

  void GetVisitCountToHost(scoped_refptr<GetVisitCountToHostRequest> request,
                           const GURL& url);

  // Computes the most recent URL(s) that the given canonical URL has
  // redirected to and returns true on success. There may be more than one
  // redirect in a row, so this function will fill the given array with the
  // entire chain. If there are no redirects for the most recent visit of the
  // URL, or the URL is not in history, returns false.
  //
  // Backend for QueryRedirectsFrom.
  bool GetMostRecentRedirectsFrom(const GURL& url,
                                  HistoryService::RedirectList* redirects);

  // Thumbnails ----------------------------------------------------------------

  void SetPageThumbnail(const GURL& url,
                        const SkBitmap& thumbnail,
                        const ThumbnailScore& score);

  // Retrieves a thumbnail, passing it across thread boundaries
  // via. the included callback.
  void GetPageThumbnail(scoped_refptr<GetPageThumbnailRequest> request,
                        const GURL& page_url);

  // Backend implementation of GetPageThumbnail. Unlike
  // GetPageThumbnail(), this method has way to transport data across
  // thread boundaries.
  //
  // Exposed for testing reasons.
  void GetPageThumbnailDirectly(
      const GURL& page_url,
      scoped_refptr<RefCountedBytes>* data);

  // Favicon -------------------------------------------------------------------

  void GetFavIcon(scoped_refptr<GetFavIconRequest> request,
                  const GURL& icon_url);
  void GetFavIconForURL(scoped_refptr<GetFavIconRequest> request,
                        const GURL& page_url);
  void SetFavIcon(const GURL& page_url,
                  const GURL& icon_url,
                  scoped_refptr<RefCountedBytes> data);
  void UpdateFavIconMappingAndFetch(scoped_refptr<GetFavIconRequest> request,
                                    const GURL& page_url,
                                    const GURL& icon_url);
  void SetFavIconOutOfDateForPage(const GURL& page_url);
  void SetImportedFavicons(
      const std::vector<ImportedFavIconUsage>& favicon_usage);

  // Downloads -----------------------------------------------------------------

  void QueryDownloads(scoped_refptr<DownloadQueryRequest> request);
  void UpdateDownload(int64 received_bytes, int32 state, int64 db_handle);
  void UpdateDownloadPath(const std::wstring& path, int64 db_handle);
  void CreateDownload(scoped_refptr<DownloadCreateRequest> request,
                      const DownloadCreateInfo& info);
  void RemoveDownload(int64 db_handle);
  void RemoveDownloadsBetween(const base::Time remove_begin,
                              const base::Time remove_end);
  void RemoveDownloads(const base::Time remove_end);
  void SearchDownloads(scoped_refptr<DownloadSearchRequest>,
                       const std::wstring& search_text);

  // Segment usage -------------------------------------------------------------

  void QuerySegmentUsage(scoped_refptr<QuerySegmentUsageRequest> request,
                         const base::Time from_time);
  void DeleteOldSegmentData();
  void SetSegmentPresentationIndex(SegmentID segment_id, int index);

  // Keyword search terms ------------------------------------------------------

  void SetKeywordSearchTermsForURL(const GURL& url,
                                   TemplateURL::IDType keyword_id,
                                   const std::wstring& term);

  void DeleteAllSearchTermsForKeyword(TemplateURL::IDType keyword_id);

  void GetMostRecentKeywordSearchTerms(
      scoped_refptr<GetMostRecentKeywordSearchTermsRequest> request,
      TemplateURL::IDType keyword_id,
      const std::wstring& prefix,
      int max_count);

  // Generic operations --------------------------------------------------------

  void ProcessDBTask(scoped_refptr<HistoryDBTaskRequest> request);

  // Deleting ------------------------------------------------------------------

  void DeleteURL(const GURL& url);

  // Calls ExpireHistoryBackend::ExpireHistoryBetween and commits the change.
  void ExpireHistoryBetween(scoped_refptr<ExpireHistoryRequest> request,
                            base::Time begin_time,
                            base::Time end_time);

  // Bookmarks -----------------------------------------------------------------

  // Notification that a URL is no longer bookmarked. If there are no visits
  // for the specified url, it is deleted.
  void URLsNoLongerBookmarked(const std::set<GURL>& urls);

  // Testing -------------------------------------------------------------------

  // Sets the task to run and the message loop to run it on when this object
  // is destroyed. See HistoryService::SetOnBackendDestroyTask for a more
  // complete description.
  void SetOnBackendDestroyTask(MessageLoop* message_loop, Task* task);

  // Adds the given rows to the database if it doesn't exist. A visit will be
  // added for each given URL at the last visit time in the URLRow.
  void AddPagesWithDetails(const std::vector<URLRow>& info);

 private:
  friend class CommitLaterTask;  // The commit task needs to call Commit().
  friend class HistoryTest;  // So the unit tests can poke our innards.
  FRIEND_TEST(HistoryBackendTest, DeleteAll);
  FRIEND_TEST(HistoryBackendTest, URLsNoLongerBookmarked);
  friend class ::TestingProfile;

  // Computes the name of the specified database on disk.
  std::wstring GetThumbnailFileName() const;
  std::wstring GetArchivedFileName() const;

  class URLQuerier;
  friend class URLQuerier;

  // Does the work of Init.
  void InitImpl();

  // Adds a single visit to the database, updating the URL information such
  // as visit and typed count. The visit ID of the added visit and the URL ID
  // of the associated URL (whether added or not) is returned. Both values will
  // be 0 on failure.
  //
  // This does not schedule database commits, it is intended to be used as a
  // subroutine for AddPage only. It also assumes the database is valid.
  std::pair<URLID, VisitID> AddPageVisit(const GURL& url,
                                         base::Time time,
                                         VisitID referring_visit,
                                         PageTransition::Type transition);

  // Returns a redirect chain in |redirects| for the VisitID
  // |cur_visit|. |cur_visit| is assumed to be valid. Assumes that
  // this HistoryBackend object has been Init()ed successfully.
  void GetRedirectsFromSpecificVisit(
      VisitID cur_visit, HistoryService::RedirectList* redirects);

  // Thumbnail Helpers ---------------------------------------------------------

  // When a simple GetMostRecentRedirectsFrom() fails, this method is
  // called which searches the last N visit sessions instead of just
  // the current one. Returns true and puts thumbnail data in |data|
  // if a proper thumbnail was found. Returns false otherwise. Assumes
  // that this HistoryBackend object has been Init()ed successfully.
  bool GetThumbnailFromOlderRedirect(
      const GURL& page_url, std::vector<unsigned char>* data);

  // Querying ------------------------------------------------------------------

  // Backends for QueryHistory. *Basic() handles queries that are not FTS (full
  // text search) queries and can just be given directly to the history DB).
  // The FTS version queries the text_database, then merges with the history DB.
  // Both functions assume QueryHistory already checked the DB for validity.
  void QueryHistoryBasic(URLDatabase* url_db, VisitDatabase* visit_db,
                         const QueryOptions& options, QueryResults* result);
  void QueryHistoryFTS(const std::wstring& text_query,
                       const QueryOptions& options,
                       QueryResults* result);

  // Committing ----------------------------------------------------------------

  // We always keep a transaction open on the history database so that multiple
  // transactions can be batched. Periodically, these are flushed (use
  // ScheduleCommit). This function does the commit to write any new changes to
  // disk and opens a new transaction. This will be called automatically by
  // ScheduleCommit, or it can be called explicitly if a caller really wants
  // to write something to disk.
  void Commit();

  // Schedules a commit to happen in the future. We do this so that many
  // operations over a period of time will be batched together. If there is
  // already a commit scheduled for the future, this will do nothing.
  void ScheduleCommit();

  // Cancels the scheduled commit, if any. If there is no scheduled commit,
  // does nothing.
  void CancelScheduledCommit();

  // Segments ------------------------------------------------------------------

  // Walks back a segment chain to find the last visit with a non null segment
  // id and returns it. If there is none found, returns 0.
  SegmentID GetLastSegmentID(VisitID from_visit);

  // Update the segment information. This is called internally when a page is
  // added. Return the segment id of the segment that has been updated.
  SegmentID UpdateSegments(const GURL& url,
                           VisitID from_visit,
                           VisitID visit_id,
                           PageTransition::Type transition_type,
                           const base::Time ts);

  // Favicons ------------------------------------------------------------------

  // Used by both UpdateFavIconMappingAndFetch and GetFavIcon.
  // If page_url is non-null and SetFavIcon has previously been invoked for
  // icon_url the favicon url for page_url (and all redirects) is set to
  // icon_url.
  void UpdateFavIconMappingAndFetchImpl(
      const GURL* page_url,
      const GURL& icon_url,
      scoped_refptr<GetFavIconRequest> request);

  // Sets the favicon url id for page_url to id. This will also broadcast
  // notifications as necessary.
  void SetFavIconMapping(const GURL& page_url, FavIconID id);

  // Generic stuff -------------------------------------------------------------

  // Processes the next scheduled HistoryDBTask, scheduling this method
  // to be invoked again if there are more tasks that need to run.
  void ProcessDBTaskImpl();

  // Release all tasks in history_db_tasks_ and clears it.
  void ReleaseDBTasks();

  // Schedules a broadcast of the given notification on the main thread. The
  // details argument will have ownership taken by this function (it will be
  // sent to the main thread and deleted there).
  void BroadcastNotifications(NotificationType type,
                              HistoryDetails* details_deleted);

  // Deleting all history ------------------------------------------------------

  // Deletes all history. This is a special case of deleting that is separated
  // from our normal dependency-following method for performance reasons. The
  // logic lives here instead of ExpireHistoryBackend since it will cause
  // re-initialization of some databases such as Thumbnails or Archived that
  // could fail. When these databases are not valid, our pointers must be NULL,
  // so we need to handle this type of operation to keep the pointers in sync.
  void DeleteAllHistory();

  // Given a vector of all URLs that we will keep, removes all thumbnails
  // referenced by any URL, and also all favicons that aren't used by those
  // URLs. The favicon IDs will change, so this will update the url rows in the
  // vector to reference the new IDs.
  bool ClearAllThumbnailHistory(std::vector<URLRow>* kept_urls);

  // Deletes all information in the history database, except for the supplied
  // set of URLs in the URL table (these should correspond to the bookmarked
  // URLs).
  //
  // The IDs of the URLs may change.
  bool ClearAllMainHistory(const std::vector<URLRow>& kept_urls);

  // Returns the BookmarkService, blocking until it is loaded. This may return
  // NULL during testing.
  BookmarkService* GetBookmarkService();

  // Data ----------------------------------------------------------------------

  // Delegate. See the class definition above for more information. This will
  // be NULL before Init is called and after Cleanup, but is guaranteed
  // non-NULL in between.
  scoped_ptr<Delegate> delegate_;

  // Directory where database files will be stored.
  std::wstring history_dir_;

  // The history/thumbnail databases. Either MAY BE NULL if the database could
  // not be opened, all users must first check for NULL and return immediately
  // if it is. The thumbnail DB may be NULL when the history one isn't, but not
  // vice-versa.
  scoped_ptr<HistoryDatabase> db_;
  scoped_ptr<ThumbnailDatabase> thumbnail_db_;

  // Stores old history in a larger, slower database.
  scoped_ptr<ArchivedDatabase> archived_db_;

  // Full text database manager, possibly NULL if the database could not be
  // created.
  scoped_ptr<TextDatabaseManager> text_database_;

  // Manages expiration between the various databases.
  ExpireHistoryBackend expirer_;

  // A commit has been scheduled to occur sometime in the future. We can check
  // non-null-ness to see if there is a commit scheduled in the future, and we
  // can use the pointer to cancel the scheduled commit. There can be only one
  // scheduled commit at a time (see ScheduleCommit).
  scoped_refptr<CommitLaterTask> scheduled_commit_;

  // Maps recent redirect destination pages to the chain of redirects that
  // brought us to there. Pages that did not have redirects or were not the
  // final redirect in a chain will not be in this list, as well as pages that
  // redirected "too long" ago (as determined by ExpireOldRedirects above).
  // It is used to set titles & favicons for redirects to that of the
  // destination.
  //
  // As with AddPage, the last item in the redirect chain will be the
  // destination of the redirect (i.e., the key into recent_redirects_);
  typedef MRUCache<GURL, HistoryService::RedirectList> RedirectCache;
  RedirectCache recent_redirects_;

  // Timestamp of the last page addition request. We use this to detect when
  // multiple additions are requested at the same time (within the resolution
  // of the timer), so we can try to ensure they're unique when they're added
  // to the database by using the last_recorded_time_ (q.v.). We still can't
  // enforce or guarantee uniqueness, since the user might set his clock back.
  base::Time last_requested_time_;

  // Timestamp of the last page addition, as it was recorded in the database.
  // If two or more requests come in at the same time, we increment that time
  // by 1 us between them so it's more likely to be unique in the database.
  // This keeps track of that higher-resolution timestamp.
  base::Time last_recorded_time_;

  // When non-NULL, this is the task that should be invoked on
  MessageLoop* backend_destroy_message_loop_;
  Task* backend_destroy_task_;

  // Tracks page transition types.
  VisitTracker tracker_;

  // A boolean variable to track whether we have already purged obsolete segment
  // data.
  bool segment_queried_;

  // HistoryDBTasks to run. Be sure to AddRef when adding, and Release when
  // done.
  std::list<HistoryDBTaskRequest*> db_task_requests_;

  // Used to determine if a URL is bookmarked. This is owned by the Profile and
  // may be NULL (during testing).
  //
  // Use GetBookmarkService to access this, which makes sure the service is
  // loaded.
  BookmarkService* bookmark_service_;

  // Publishes the history to all indexers which are registered to receive
  // history data from us. Can be NULL if there are no listeners.
  scoped_ptr<HistoryPublisher> history_publisher_;

  DISALLOW_EVIL_CONSTRUCTORS(HistoryBackend);
};

}  // namespace history

#endif  // CHROME_BROWSER_HISTORY_HISTORY_BACKEND_H__
