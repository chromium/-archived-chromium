// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_EXPIRE_HISTORY_BACKEND_H__
#define CHROME_BROWSER_HISTORY_EXPIRE_HISTORY_BACKEND_H__

#include <set>
#include <vector>

#include "base/basictypes.h"
#include "base/task.h"
#include "base/time.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/history/text_database_manager.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

class BookmarkService;
class GURL;
class NotificationType;

namespace history {

class ArchivedDatabase;
class HistoryDatabase;
struct HistoryDetails;
class ThumbnailDatabase;

// Delegate used to broadcast notifications to the main thread.
class BroadcastNotificationDelegate {
 public:
  // Schedules a broadcast of the given notification on the application main
  // thread. The details argument will have ownership taken by this function.
  virtual void BroadcastNotifications(NotificationType type,
                                      HistoryDetails* details_deleted) = 0;
};

// Helper component to HistoryBackend that manages expiration and deleting of
// history, as well as moving data from the main database to the archived
// database as it gets old.
//
// It will automatically start periodically archiving old history once you call
// StartArchivingOldStuff().
class ExpireHistoryBackend {
 public:
  // The delegate pointer must be non-NULL. We will NOT take ownership of it.
  // BookmarkService may be NULL. The BookmarkService is used when expiring
  // URLs so that we don't remove any URLs or favicons that are bookmarked
  // (visits are removed though).
  ExpireHistoryBackend(BroadcastNotificationDelegate* delegate,
                       BookmarkService* bookmark_service);
  ~ExpireHistoryBackend();

  // Completes initialization by setting the databases that this class will use.
  void SetDatabases(HistoryDatabase* main_db,
                    ArchivedDatabase* archived_db,
                    ThumbnailDatabase* thumb_db,
                    TextDatabaseManager* text_db);

  // Begins periodic expiration of history older than the given threshold. This
  // will continue until the object is deleted.
  void StartArchivingOldStuff(base::TimeDelta expiration_threshold);

  // Deletes everything associated with a URL.
  void DeleteURL(const GURL& url);

  // Removes all visits in the given time range, updating the URLs accordingly.
  void ExpireHistoryBetween(base::Time begin_time, base::Time end_time);

  // Archives all visits before and including the given time, updating the URLs
  // accordingly. This function is intended for migrating old databases
  // (which encompased all time) to the tiered structure and testing, and
  // probably isn't useful for anything else.
  void ArchiveHistoryBefore(base::Time end_time);

  // Returns the current time that we are archiving stuff to. This will return
  // the threshold in absolute time rather than a delta, so the caller should
  // not save it.
  base::Time GetCurrentArchiveTime() const {
    return base::Time::Now() - expiration_threshold_;
  }

 private:
  //friend class ExpireHistoryTest_DeleteFaviconsIfPossible_Test;
  FRIEND_TEST(ExpireHistoryTest, DeleteTextIndexForURL);
  FRIEND_TEST(ExpireHistoryTest, DeleteFaviconsIfPossible);
  FRIEND_TEST(ExpireHistoryTest, ArchiveSomeOldHistory);
  friend class TestingProfile;

  struct DeleteDependencies {
    // The time range affected. These can be is_null() to be unbounded in one
    // or both directions.
    base::Time begin_time, end_time;

    // ----- Filled by DeleteVisitRelatedInfo or manually if a function doesn't
    //       call that function. -----

    // The unique URL rows affected by this delete.
    std::map<URLID, URLRow> affected_urls;

    // ----- Filled by DeleteOneURL -----

    // The URLs deleted during this operation.
    std::vector<URLRow> deleted_urls;

    // The list of all favicon IDs that the affected URLs had. Favicons will be
    // shared between all URLs with the same favicon, so this is the set of IDs
    // that we will need to check when the delete operations are complete.
    std::set<FavIconID> affected_favicons;

    // Tracks the set of databases that have changed so we can optimize when
    // when we're done.
    TextDatabaseManager::ChangeSet text_db_changes;
  };

  // Removes the data from the full text index associated with the given URL
  // string/ID pair. If |update_visits| is set, the visits that reference the
  // indexed data will be updated to reflect the fact that the indexed data is
  // gone. Setting this to false is a performance optimization when the caller
  // knows that the visits will be deleted after the call.
  //
  // TODO(brettw) when we have an "archived" history database, this should take
  // a flag to optionally delete from there. This way it can be used for page
  // re-indexing as well as for full URL deletion.
  void DeleteTextIndexForURL(const GURL& url, URLID url_id, bool update_visits);

  // Deletes the visit-related stuff for all the visits in the given list, and
  // adds the rows for unique URLs affected to the affected_urls list in
  // the dependencies structure.
  //
  // Deleted information is the visits themselves and the full-text index
  // entries corresponding to them.
  void DeleteVisitRelatedInfo(const VisitVector& visits,
                              DeleteDependencies* dependencies);

  // Moves the given visits from the main database to the archived one.
  void ArchiveVisits(const VisitVector& visits);

  // Finds or deletes dependency information for the given URL. Information that
  // is specific to this URL (URL row, thumbnails, full text indexed stuff,
  // etc.) is deleted.
  //
  // This does not affect the visits! This is used for expiration as well as
  // deleting from the UI, and they handle visits differently.
  //
  // Other information will be collected and returned in the output containers.
  // This includes some of the things deleted that are needed elsewhere, plus
  // some things like favicons that could be shared by many URLs, and need to
  // be checked for deletion (this allows us to delete many URLs with only one
  // check for shared information at the end).
  //
  // Assumes the main_db_ is non-NULL.
  //
  // NOTE: If the url is bookmarked only the segments and text db are updated,
  // everything else is unchanged. This is done so that bookmarks retain their
  // favicons and thumbnails.
  void DeleteOneURL(const URLRow& url_row,
                    bool is_bookmarked,
                    DeleteDependencies* dependencies);

  // Adds or merges the given URL row with the archived database, returning the
  // ID of the URL in the archived database, or 0 on failure. The main (source)
  // database will not be affected (the URL will have to be deleted later).
  //
  // Assumes the archived database is not NULL.
  URLID ArchiveOneURL(const URLRow& url_row);

  // Deletes all the URLs in the given vector and handles their dependencies.
  // This will delete starred URLs
  void DeleteURLs(const std::vector<URLRow>& urls,
                  DeleteDependencies* dependencies);

  // Expiration involves removing visits, then propogating the visits out from
  // there and delete any orphaned URLs. These will be added to the deleted URLs
  // field of the dependencies and DeleteOneURL will handle deleting out from
  // there. This function does not handle favicons.
  //
  // When a URL is not deleted and |archive| is not set, the last visit time and
  // the visit and typed counts will be updated (we want to clear these when a
  // user is deleting history manually, but not when we're normally expiring old
  // things from history).
  //
  // The visits in the given vector should have already been deleted from the
  // database, and the list of affected URLs already be filled into
  // |depenencies->affected_urls|.
  //
  // Starred URLs will not be deleted. The information in the dependencies that
  // DeleteOneURL fills in will be updated, and this function will also delete
  // any now-unused favicons.
  void ExpireURLsForVisits(const VisitVector& visits,
                           DeleteDependencies* dependencies);

  // Creates entries in the archived database for the unique URLs referenced
  // by the given visits. It will then add versions of the visits to that
  // database. The source database WILL NOT BE MODIFIED. The source URLs and
  // visits will have to be deleted in another pass.
  //
  // The affected URLs will be filled into the given dependencies structure.
  void ArchiveURLsAndVisits(const VisitVector& visits,
                            DeleteDependencies* dependencies);

  // Deletes the favicons listed in the set if unused. Fails silently (we don't
  // care about favicons so much, so don't want to stop everything if it fails).
  void DeleteFaviconsIfPossible(const std::set<FavIconID>& favicon_id);

  // Broadcast the URL deleted notification.
  void BroadcastDeleteNotifications(DeleteDependencies* dependencies);

  // Schedules a call to DoArchiveIteration at the given time in the
  // future.
  void ScheduleArchive(base::TimeDelta delay);

  // Calls ArchiveSomeOldHistory to expire some amount of old history, and
  // schedules another call to happen in the future.
  void DoArchiveIteration();

  // Tries to expire the oldest |max_visits| visits from history that are older
  // than |time_threshold|. The return value indicates if we think there might
  // be more history to expire with the current time threshold (it does not
  // indicate success or failure).
  bool ArchiveSomeOldHistory(base::Time time_threshold, int max_visits);

  // Tries to detect possible bad history or inconsistencies in the database
  // and deletes items. For example, URLs with no visits.
  void ParanoidExpireHistory();

  // Returns the BookmarkService, blocking until it is loaded. This may return
  // NULL.
  BookmarkService* GetBookmarkService();

  // Non-owning pointer to the notification delegate (guaranteed non-NULL).
  BroadcastNotificationDelegate* delegate_;

  // Non-owning pointers to the databases we deal with (MAY BE NULL).
  HistoryDatabase* main_db_;       // Main history database.
  ArchivedDatabase* archived_db_;  // Old history.
  ThumbnailDatabase* thumb_db_;    // Thumbnails and favicons.
  TextDatabaseManager* text_db_;   // Full text index.

  // Used to generate runnable methods to do timers on this class. They will be
  // automatically canceled when this class is deleted.
  ScopedRunnableMethodFactory<ExpireHistoryBackend> factory_;

  // The threshold for "old" history where we will automatically expire it to
  // the archived database.
  base::TimeDelta expiration_threshold_;

  // The BookmarkService; may be null. This is owned by the Profile.
  //
  // Use GetBookmarkService to access this, which makes sure the service is
  // loaded.
  BookmarkService* bookmark_service_;

  DISALLOW_EVIL_CONSTRUCTORS(ExpireHistoryBackend);
};

}  // namespace history

#endif  // CHROME_BROWSER_HISTORY_EXPIRE_HISTORY_BACKEND_H__
