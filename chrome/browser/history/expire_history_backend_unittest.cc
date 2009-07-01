// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/gfx/jpeg_codec.h"
#include "base/path_service.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/history/archived_database.h"
#include "chrome/browser/history/expire_history_backend.h"
#include "chrome/browser/history/history_database.h"
#include "chrome/browser/history/text_database_manager.h"
#include "chrome/browser/history/thumbnail_database.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/thumbnail_score.h"
#include "chrome/tools/profiles/thumbnail-inl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"

using base::Time;
using base::TimeDelta;
using base::TimeTicks;

// Filename constants.
static const FilePath::CharType kTestDir[] = FILE_PATH_LITERAL("ExpireTest");
static const FilePath::CharType kHistoryFile[] = FILE_PATH_LITERAL("History");
static const FilePath::CharType kArchivedHistoryFile[] =
    FILE_PATH_LITERAL("Archived History");
static const FilePath::CharType kThumbnailFile[] =
    FILE_PATH_LITERAL("Thumbnails");

// The test must be in the history namespace for the gtest forward declarations
// to work. It also eliminates a bunch of ugly "history::".
namespace history {

// ExpireHistoryTest -----------------------------------------------------------

class ExpireHistoryTest : public testing::Test,
                          public BroadcastNotificationDelegate {
 public:
  ExpireHistoryTest()
      : bookmark_model_(NULL),
        ALLOW_THIS_IN_INITIALIZER_LIST(expirer_(this, &bookmark_model_)),
        now_(Time::Now()) {
  }

 protected:
  // Called by individual tests when they want data populated.
  void AddExampleData(URLID url_ids[3], Time visit_times[4]);

  // Returns true if the given favicon/thumanil has an entry in the DB.
  bool HasFavIcon(FavIconID favicon_id);
  bool HasThumbnail(URLID url_id);

  // Returns the number of text matches for the given URL in the example data
  // added by AddExampleData.
  int CountTextMatchesForURL(const GURL& url);

  // EXPECTs that each URL-specific history thing (basically, everything but
  // favicons) is gone.
  void EnsureURLInfoGone(const URLRow& row);

  // Clears the list of notifications received.
  void ClearLastNotifications() {
    for (size_t i = 0; i < notifications_.size(); i++)
      delete notifications_[i].second;
    notifications_.clear();
  }

  void StarURL(const GURL& url) {
    bookmark_model_.AddURL(
        bookmark_model_.GetBookmarkBarNode(), 0, std::wstring(), url);
  }

  static bool IsStringInFile(const FilePath& filename, const char* str);

  BookmarkModel bookmark_model_;

  MessageLoop message_loop_;

  ExpireHistoryBackend expirer_;

  scoped_ptr<HistoryDatabase> main_db_;
  scoped_ptr<ArchivedDatabase> archived_db_;
  scoped_ptr<ThumbnailDatabase> thumb_db_;
  scoped_ptr<TextDatabaseManager> text_db_;

  // Time at the beginning of the test, so everybody agrees what "now" is.
  const Time now_;

  // Notifications intended to be broadcast, we can check these values to make
  // sure that the deletor is doing the correct broadcasts. We own the details
  // pointers.
  typedef std::vector< std::pair<NotificationType, HistoryDetails*> >
      NotificationList;
  NotificationList notifications_;

  // Directory for the history files.
  FilePath dir_;

 private:
  void SetUp() {
    FilePath temp_dir;
    PathService::Get(base::DIR_TEMP, &temp_dir);
    dir_ = temp_dir.Append(kTestDir);
    file_util::Delete(dir_, true);
    file_util::CreateDirectory(dir_);

    FilePath history_name = dir_.Append(kHistoryFile);
    main_db_.reset(new HistoryDatabase);
    if (main_db_->Init(history_name, FilePath()) !=
        INIT_OK)
      main_db_.reset();

    FilePath archived_name = dir_.Append(kArchivedHistoryFile);
    archived_db_.reset(new ArchivedDatabase);
    if (!archived_db_->Init(archived_name))
      archived_db_.reset();

    FilePath thumb_name = dir_.Append(kThumbnailFile);
    thumb_db_.reset(new ThumbnailDatabase);
    if (thumb_db_->Init(thumb_name, NULL) != INIT_OK)
      thumb_db_.reset();

    text_db_.reset(new TextDatabaseManager(dir_,
                                           main_db_.get(), main_db_.get()));
    if (!text_db_->Init(NULL))
      text_db_.reset();

    expirer_.SetDatabases(main_db_.get(), archived_db_.get(), thumb_db_.get(),
                          text_db_.get());
  }

  void TearDown() {
    ClearLastNotifications();

    expirer_.SetDatabases(NULL, NULL, NULL, NULL);

    main_db_.reset();
    archived_db_.reset();
    thumb_db_.reset();
    text_db_.reset();
    file_util::Delete(dir_, true);
  }

  // BroadcastNotificationDelegate implementation.
  void BroadcastNotifications(NotificationType type,
                              HistoryDetails* details_deleted) {
    // This gets called when there are notifications to broadcast. Instead, we
    // store them so we can tell that the correct notifications were sent.
    notifications_.push_back(std::make_pair(type, details_deleted));
  }
};

// The example data consists of 4 visits. The middle two visits are to the
// same URL, while the first and last are for unique ones. This allows a test
// for the oldest or newest to include both a URL that should get totally
// deleted (the one on the end) with one that should only get a visit deleted
// (with the one in the middle) when it picks the proper threshold time.
//
// Each visit has indexed data, each URL has thumbnail. The first two URLs will
// share the same favicon, while the last one will have a unique favicon. The
// second visit for the middle URL is typed.
//
// The IDs of the added URLs, and the times of the four added visits will be
// added to the given arrays.
void ExpireHistoryTest::AddExampleData(URLID url_ids[3], Time visit_times[4]) {
  if (!main_db_.get() || !text_db_.get())
    return;

  // Four times for each visit.
  visit_times[3] = Time::Now();
  visit_times[2] = visit_times[3] - TimeDelta::FromDays(1);
  visit_times[1] = visit_times[3] - TimeDelta::FromDays(2);
  visit_times[0] = visit_times[3] - TimeDelta::FromDays(3);

  // Two favicons. The first two URLs will share the same one, while the last
  // one will have a unique favicon.
  FavIconID favicon1 = thumb_db_->AddFavIcon(GURL("http://favicon/url1"));
  FavIconID favicon2 = thumb_db_->AddFavIcon(GURL("http://favicon/url2"));

  // Three URLs.
  URLRow url_row1(GURL("http://www.google.com/1"));
  url_row1.set_last_visit(visit_times[0]);
  url_row1.set_favicon_id(favicon1);
  url_row1.set_visit_count(1);
  url_ids[0] = main_db_->AddURL(url_row1);

  URLRow url_row2(GURL("http://www.google.com/2"));
  url_row2.set_last_visit(visit_times[2]);
  url_row2.set_favicon_id(favicon1);
  url_row2.set_visit_count(2);
  url_row2.set_typed_count(1);
  url_ids[1] = main_db_->AddURL(url_row2);

  URLRow url_row3(GURL("http://www.google.com/3"));
  url_row3.set_last_visit(visit_times[3]);
  url_row3.set_favicon_id(favicon2);
  url_row3.set_visit_count(1);
  url_ids[2] = main_db_->AddURL(url_row3);

  // Thumbnails for each URL.
  scoped_ptr<SkBitmap> thumbnail(
      JPEGCodec::Decode(kGoogleThumbnail, sizeof(kGoogleThumbnail)));
  ThumbnailScore score(0.25, true, true, Time::Now());

  Time time;
  GURL gurl;
  thumb_db_->SetPageThumbnail(gurl, url_ids[0], *thumbnail, score, time);
  thumb_db_->SetPageThumbnail(gurl, url_ids[1], *thumbnail, score, time);
  thumb_db_->SetPageThumbnail(gurl, url_ids[2], *thumbnail, score, time);

  // Four visits.
  VisitRow visit_row1;
  visit_row1.url_id = url_ids[0];
  visit_row1.visit_time = visit_times[0];
  visit_row1.is_indexed = true;
  main_db_->AddVisit(&visit_row1);

  VisitRow visit_row2;
  visit_row2.url_id = url_ids[1];
  visit_row2.visit_time = visit_times[1];
  visit_row2.is_indexed = true;
  main_db_->AddVisit(&visit_row2);

  VisitRow visit_row3;
  visit_row3.url_id = url_ids[1];
  visit_row3.visit_time = visit_times[2];
  visit_row3.is_indexed = true;
  visit_row3.transition = PageTransition::TYPED;
  main_db_->AddVisit(&visit_row3);

  VisitRow visit_row4;
  visit_row4.url_id = url_ids[2];
  visit_row4.visit_time = visit_times[3];
  visit_row4.is_indexed = true;
  main_db_->AddVisit(&visit_row4);

  // Full text index for each visit.
  text_db_->AddPageData(url_row1.url(), visit_row1.url_id, visit_row1.visit_id,
                        visit_row1.visit_time, L"title", L"body");

  text_db_->AddPageData(url_row2.url(), visit_row2.url_id, visit_row2.visit_id,
                        visit_row2.visit_time, L"title", L"body");
  text_db_->AddPageData(url_row2.url(), visit_row3.url_id, visit_row3.visit_id,
                        visit_row3.visit_time, L"title", L"body");

  // Note the special text in this URL. We'll search the file for this string
  // to make sure it doesn't hang around after the delete.
  text_db_->AddPageData(url_row3.url(), visit_row4.url_id, visit_row4.visit_id,
                        visit_row4.visit_time, L"title", L"goats body");
}

bool ExpireHistoryTest::HasFavIcon(FavIconID favicon_id) {
  if (!thumb_db_.get())
    return false;
  Time last_updated;
  std::vector<unsigned char> icon_data_unused;
  GURL icon_url;
  return thumb_db_->GetFavIcon(favicon_id, &last_updated, &icon_data_unused,
                               &icon_url);
}

bool ExpireHistoryTest::HasThumbnail(URLID url_id) {
  std::vector<unsigned char> temp_data;
  return thumb_db_->GetPageThumbnail(url_id, &temp_data);
}

int ExpireHistoryTest::CountTextMatchesForURL(const GURL& url) {
  if (!text_db_.get())
    return 0;

  // "body" should match all pagesx in the example data.
  std::vector<TextDatabase::Match> results;
  QueryOptions options;
  options.most_recent_visit_only = false;
  Time first_time;
  text_db_->GetTextMatches(L"body", options, &results, &first_time);

  int count = 0;
  for (size_t i = 0; i < results.size(); i++) {
    if (results[i].url == url)
      count++;
  }
  return count;
}

void ExpireHistoryTest::EnsureURLInfoGone(const URLRow& row) {
  // Verify the URL no longer exists.
  URLRow temp_row;
  EXPECT_FALSE(main_db_->GetURLRow(row.id(), &temp_row));

  // The indexed data should be gone.
  EXPECT_EQ(0, CountTextMatchesForURL(row.url()));

  // There should be no visits.
  VisitVector visits;
  main_db_->GetVisitsForURL(row.id(), &visits);
  EXPECT_EQ(0U, visits.size());

  // Thumbnail should be gone.
  EXPECT_FALSE(HasThumbnail(row.id()));

  // Check the notifications. There should be a delete notification with this
  // URL in it. There should also be a "typed URL changed" notification if the
  // row is marked typed.
  bool found_delete_notification = false;
  bool found_typed_changed_notification = false;
  for (size_t i = 0; i < notifications_.size(); i++) {
    if (notifications_[i].first == NotificationType::HISTORY_URLS_DELETED) {
      const URLsDeletedDetails* deleted_details =
          reinterpret_cast<URLsDeletedDetails*>(notifications_[i].second);
      if (deleted_details->urls.find(row.url()) !=
          deleted_details->urls.end()) {
        found_delete_notification = true;
      }
    } else if (notifications_[i].first ==
               NotificationType::HISTORY_TYPED_URLS_MODIFIED) {
      // See if we got a typed URL changed notification.
      const URLsModifiedDetails* modified_details =
          reinterpret_cast<URLsModifiedDetails*>(notifications_[i].second);
      for (size_t cur_url = 0; cur_url < modified_details->changed_urls.size();
           cur_url++) {
        if (modified_details->changed_urls[cur_url].url() == row.url())
          found_typed_changed_notification = true;
      }
    } else if (notifications_[i].first ==
               NotificationType::HISTORY_URL_VISITED) {
      // See if we got a visited URL notification.
      const URLVisitedDetails* visited_details =
          reinterpret_cast<URLVisitedDetails*>(notifications_[i].second);
      if (visited_details->row.url() == row.url())
          found_typed_changed_notification = true;
    }
  }
  EXPECT_TRUE(found_delete_notification);
  EXPECT_EQ(row.typed_count() > 0, found_typed_changed_notification);
}

TEST_F(ExpireHistoryTest, DeleteFaviconsIfPossible) {
  // Add a favicon record.
  const GURL favicon_url("http://www.google.com/favicon.ico");
  FavIconID icon_id = thumb_db_->AddFavIcon(favicon_url);
  EXPECT_TRUE(icon_id);
  EXPECT_TRUE(HasFavIcon(icon_id));

  // The favicon should be deletable with no users.
  std::set<FavIconID> favicon_set;
  favicon_set.insert(icon_id);
  expirer_.DeleteFaviconsIfPossible(favicon_set);
  EXPECT_FALSE(HasFavIcon(icon_id));

  // Add back the favicon.
  icon_id = thumb_db_->AddFavIcon(favicon_url);
  EXPECT_TRUE(icon_id);
  EXPECT_TRUE(HasFavIcon(icon_id));

  // Add a page that references the favicon.
  URLRow row(GURL("http://www.google.com/2"));
  row.set_visit_count(1);
  row.set_favicon_id(icon_id);
  EXPECT_TRUE(main_db_->AddURL(row));

  // Favicon should not be deletable.
  favicon_set.clear();
  favicon_set.insert(icon_id);
  expirer_.DeleteFaviconsIfPossible(favicon_set);
  EXPECT_TRUE(HasFavIcon(icon_id));
}

// static
bool ExpireHistoryTest::IsStringInFile(const FilePath& filename,
                                       const char* str) {
  std::string contents;
  EXPECT_TRUE(file_util::ReadFileToString(filename, &contents));
  return contents.find(str) != std::string::npos;
}

// Deletes a URL with a favicon that it is the last referencer of, so that it
// should also get deleted.
// Temporarily disabling as fails near end of month.
TEST_F(ExpireHistoryTest, DISABLED_DeleteURLAndFavicon) {
  URLID url_ids[3];
  Time visit_times[4];
  AddExampleData(url_ids, visit_times);

  // Verify things are the way we expect with a URL row, favicon, thumbnail.
  URLRow last_row;
  ASSERT_TRUE(main_db_->GetURLRow(url_ids[2], &last_row));
  EXPECT_TRUE(HasFavIcon(last_row.favicon_id()));
  EXPECT_TRUE(HasThumbnail(url_ids[2]));

  VisitVector visits;
  main_db_->GetVisitsForURL(url_ids[2], &visits);
  ASSERT_EQ(1U, visits.size());
  EXPECT_EQ(1, CountTextMatchesForURL(last_row.url()));

  // In this test we also make sure that any pending entries in the text
  // database manager are removed.
  text_db_->AddPageURL(last_row.url(), last_row.id(), visits[0].visit_id,
                       visits[0].visit_time);

  // Compute the text DB filename.
  FilePath fts_filename = dir_.Append(
      TextDatabase::IDToFileName(text_db_->TimeToID(visit_times[3])));

  // When checking the file, the database must be closed. We then re-initialize
  // it just like the test set-up did.
  text_db_.reset();
  EXPECT_TRUE(IsStringInFile(fts_filename, "goats"));
  text_db_.reset(new TextDatabaseManager(dir_,
                                         main_db_.get(), main_db_.get()));
  ASSERT_TRUE(text_db_->Init(NULL));
  expirer_.SetDatabases(main_db_.get(), archived_db_.get(), thumb_db_.get(),
                        text_db_.get());

  // Delete the URL and its dependencies.
  expirer_.DeleteURL(last_row.url());

  // The string should be removed from the file. FTS can mark it as gone but
  // doesn't remove it from the file, we want to be sure we're doing the latter.
  text_db_.reset();
  EXPECT_FALSE(IsStringInFile(fts_filename, "goats"));
  text_db_.reset(new TextDatabaseManager(dir_,
                                         main_db_.get(), main_db_.get()));
  ASSERT_TRUE(text_db_->Init(NULL));
  expirer_.SetDatabases(main_db_.get(), archived_db_.get(), thumb_db_.get(),
                        text_db_.get());

  // Run the text database expirer. This will flush any pending entries so we
  // can check that nothing was committed. We use a time far in the future so
  // that anything added recently will get flushed.
  TimeTicks expiration_time = TimeTicks::Now() + TimeDelta::FromDays(1);
  text_db_->FlushOldChangesForTime(expiration_time);

  // All the normal data + the favicon should be gone.
  EnsureURLInfoGone(last_row);
  EXPECT_FALSE(HasFavIcon(last_row.favicon_id()));
}

// Deletes a URL with a favicon that other URLs reference, so that the favicon
// should not get deleted. This also tests deleting more than one visit.
TEST_F(ExpireHistoryTest, DeleteURLWithoutFavicon) {
  URLID url_ids[3];
  Time visit_times[4];
  AddExampleData(url_ids, visit_times);

  // Verify things are the way we expect with a URL row, favicon, thumbnail.
  URLRow last_row;
  ASSERT_TRUE(main_db_->GetURLRow(url_ids[1], &last_row));
  EXPECT_TRUE(HasFavIcon(last_row.favicon_id()));
  EXPECT_TRUE(HasThumbnail(url_ids[1]));

  VisitVector visits;
  main_db_->GetVisitsForURL(url_ids[1], &visits);
  EXPECT_EQ(2U, visits.size());
  EXPECT_EQ(1, CountTextMatchesForURL(last_row.url()));

  // Delete the URL and its dependencies.
  expirer_.DeleteURL(last_row.url());

  // All the normal data + the favicon should be gone.
  EnsureURLInfoGone(last_row);
  EXPECT_TRUE(HasFavIcon(last_row.favicon_id()));
}

// DeleteURL should not delete starred urls.
TEST_F(ExpireHistoryTest, DontDeleteStarredURL) {
  URLID url_ids[3];
  Time visit_times[4];
  AddExampleData(url_ids, visit_times);

  URLRow url_row;
  ASSERT_TRUE(main_db_->GetURLRow(url_ids[2], &url_row));

  // Star the last URL.
  StarURL(url_row.url());

  // Attempt to delete the url.
  expirer_.DeleteURL(url_row.url());

  // Because the url is starred, it shouldn't be deleted.
  GURL url = url_row.url();
  ASSERT_TRUE(main_db_->GetRowForURL(url, &url_row));

  // And the favicon should exist.
  EXPECT_TRUE(HasFavIcon(url_row.favicon_id()));

  // But there should be no fts.
  ASSERT_EQ(0, CountTextMatchesForURL(url_row.url()));

  // And no visits.
  VisitVector visits;
  main_db_->GetVisitsForURL(url_row.id(), &visits);
  ASSERT_EQ(0U, visits.size());

  // Should still have the thumbnail.
  ASSERT_TRUE(HasThumbnail(url_row.id()));

  // Unstar the URL and delete again.
  bookmark_model_.SetURLStarred(url, std::wstring(), false);
  expirer_.DeleteURL(url);

  // Now it should be completely deleted.
  EnsureURLInfoGone(url_row);
}

// Expires all URLs more recent than a given time, with no starred items.
// Our time threshold is such that one URL should be updated (we delete one of
// the two visits) and one is deleted.
TEST_F(ExpireHistoryTest, FlushRecentURLsUnstarred) {
  URLID url_ids[3];
  Time visit_times[4];
  AddExampleData(url_ids, visit_times);

  URLRow url_row1, url_row2;
  ASSERT_TRUE(main_db_->GetURLRow(url_ids[1], &url_row1));
  ASSERT_TRUE(main_db_->GetURLRow(url_ids[2], &url_row2));

  // In this test we also make sure that any pending entries in the text
  // database manager are removed.
  VisitVector visits;
  main_db_->GetVisitsForURL(url_ids[2], &visits);
  ASSERT_EQ(1U, visits.size());
  text_db_->AddPageURL(url_row2.url(), url_row2.id(), visits[0].visit_id,
                       visits[0].visit_time);

  // This should delete the last two visits.
  expirer_.ExpireHistoryBetween(visit_times[2], Time());

  // Run the text database expirer. This will flush any pending entries so we
  // can check that nothing was committed. We use a time far in the future so
  // that anything added recently will get flushed.
  TimeTicks expiration_time = TimeTicks::Now() + TimeDelta::FromDays(1);
  text_db_->FlushOldChangesForTime(expiration_time);

  // Verify that the middle URL had its last visit deleted only.
  visits.clear();
  main_db_->GetVisitsForURL(url_ids[1], &visits);
  EXPECT_EQ(1U, visits.size());
  EXPECT_EQ(0, CountTextMatchesForURL(url_row1.url()));

  // Verify that the middle URL visit time and visit counts were updated.
  URLRow temp_row;
  ASSERT_TRUE(main_db_->GetURLRow(url_ids[1], &temp_row));
  EXPECT_TRUE(visit_times[2] == url_row1.last_visit());  // Previous value.
  EXPECT_TRUE(visit_times[1] == temp_row.last_visit());  // New value.
  EXPECT_EQ(2, url_row1.visit_count());
  EXPECT_EQ(1, temp_row.visit_count());
  EXPECT_EQ(1, url_row1.typed_count());
  EXPECT_EQ(0, temp_row.typed_count());

  // Verify that the middle URL's favicon and thumbnail is still there.
  EXPECT_TRUE(HasFavIcon(url_row1.favicon_id()));
  EXPECT_TRUE(HasThumbnail(url_row1.id()));

  // Verify that the last URL was deleted.
  EnsureURLInfoGone(url_row2);
  EXPECT_FALSE(HasFavIcon(url_row2.favicon_id()));
}

// Expire a starred URL, it shouldn't get deleted
TEST_F(ExpireHistoryTest, FlushRecentURLsStarred) {
  URLID url_ids[3];
  Time visit_times[4];
  AddExampleData(url_ids, visit_times);

  URLRow url_row1, url_row2;
  ASSERT_TRUE(main_db_->GetURLRow(url_ids[1], &url_row1));
  ASSERT_TRUE(main_db_->GetURLRow(url_ids[2], &url_row2));

  // Star the last two URLs.
  StarURL(url_row1.url());
  StarURL(url_row2.url());

  // This should delete the last two visits.
  expirer_.ExpireHistoryBetween(visit_times[2], Time());

  // The URL rows should still exist.
  URLRow new_url_row1, new_url_row2;
  ASSERT_TRUE(main_db_->GetURLRow(url_ids[1], &new_url_row1));
  ASSERT_TRUE(main_db_->GetURLRow(url_ids[2], &new_url_row2));

  // The visit times should be updated.
  EXPECT_TRUE(new_url_row1.last_visit() == visit_times[1]);
  EXPECT_TRUE(new_url_row2.last_visit().is_null());  // No last visit time.

  // Visit/typed count should not be updated for bookmarks.
  EXPECT_EQ(0, new_url_row1.typed_count());
  EXPECT_EQ(1, new_url_row1.visit_count());
  EXPECT_EQ(0, new_url_row2.typed_count());
  EXPECT_EQ(0, new_url_row2.visit_count());

  // Thumbnails and favicons should still exist. Note that we keep thumbnails
  // that may have been updated since the time threshold. Since the URL still
  // exists in history, this should not be a privacy problem, we only update
  // the visit counts in this case for consistency anyway.
  EXPECT_TRUE(HasFavIcon(new_url_row1.favicon_id()));
  EXPECT_TRUE(HasThumbnail(new_url_row1.id()));
  EXPECT_TRUE(HasFavIcon(new_url_row2.favicon_id()));
  EXPECT_TRUE(HasThumbnail(new_url_row2.id()));
}

TEST_F(ExpireHistoryTest, ArchiveHistoryBeforeUnstarred) {
  URLID url_ids[3];
  Time visit_times[4];
  AddExampleData(url_ids, visit_times);

  URLRow url_row1, url_row2;
  ASSERT_TRUE(main_db_->GetURLRow(url_ids[1], &url_row1));
  ASSERT_TRUE(main_db_->GetURLRow(url_ids[2], &url_row2));

  // Archive the oldest two visits. This will actually result in deleting them
  // since their transition types are empty (not important).
  expirer_.ArchiveHistoryBefore(visit_times[1]);

  // The first URL should be deleted, the second should not be affected.
  URLRow temp_row;
  EXPECT_FALSE(main_db_->GetURLRow(url_ids[0], &temp_row));
  EXPECT_TRUE(main_db_->GetURLRow(url_ids[1], &temp_row));
  EXPECT_TRUE(main_db_->GetURLRow(url_ids[2], &temp_row));

  // Make sure the archived database has nothing in it.
  EXPECT_FALSE(archived_db_->GetRowForURL(url_row1.url(), NULL));
  EXPECT_FALSE(archived_db_->GetRowForURL(url_row2.url(), NULL));

  // Now archive one more visit so that the middle URL should be removed. This
  // one will actually be archived instead of deleted.
  expirer_.ArchiveHistoryBefore(visit_times[2]);
  EXPECT_FALSE(main_db_->GetURLRow(url_ids[1], &temp_row));
  EXPECT_TRUE(main_db_->GetURLRow(url_ids[2], &temp_row));

  // Make sure the archived database has an entry for the second URL.
  URLRow archived_row;
  // Note that the ID is different in the archived DB, so look up by URL.
  EXPECT_TRUE(archived_db_->GetRowForURL(url_row1.url(), &archived_row));
  VisitVector archived_visits;
  archived_db_->GetVisitsForURL(archived_row.id(), &archived_visits);
  EXPECT_EQ(1U, archived_visits.size());
}

TEST_F(ExpireHistoryTest, ArchiveHistoryBeforeStarred) {
  URLID url_ids[3];
  Time visit_times[4];
  AddExampleData(url_ids, visit_times);

  URLRow url_row0, url_row1;
  ASSERT_TRUE(main_db_->GetURLRow(url_ids[0], &url_row0));
  ASSERT_TRUE(main_db_->GetURLRow(url_ids[1], &url_row1));

  // Star the URLs.
  StarURL(url_row0.url());
  StarURL(url_row1.url());

  // Now archive the first three visits (first two URLs). The first two visits
  // should be, the third deleted, but the URL records should not.
  expirer_.ArchiveHistoryBefore(visit_times[2]);

  // The first URL should have its visit deleted, but it should still be present
  // in the main DB and not in the archived one since it is starred.
  URLRow temp_row;
  ASSERT_TRUE(main_db_->GetURLRow(url_ids[0], &temp_row));
  // Note that the ID is different in the archived DB, so look up by URL.
  EXPECT_FALSE(archived_db_->GetRowForURL(temp_row.url(), NULL));
  VisitVector visits;
  main_db_->GetVisitsForURL(temp_row.id(), &visits);
  EXPECT_EQ(0U, visits.size());

  // The second URL should have its first visit deleted and its second visit
  // archived. It should be present in both the main DB (because it's starred)
  // and the archived DB (for the archived visit).
  ASSERT_TRUE(main_db_->GetURLRow(url_ids[1], &temp_row));
  main_db_->GetVisitsForURL(temp_row.id(), &visits);
  EXPECT_EQ(0U, visits.size());

  // Note that the ID is different in the archived DB, so look up by URL.
  ASSERT_TRUE(archived_db_->GetRowForURL(temp_row.url(), &temp_row));
  archived_db_->GetVisitsForURL(temp_row.id(), &visits);
  ASSERT_EQ(1U, visits.size());
  EXPECT_TRUE(visit_times[2] == visits[0].visit_time);

  // The third URL should be unchanged.
  EXPECT_TRUE(main_db_->GetURLRow(url_ids[2], &temp_row));
  EXPECT_FALSE(archived_db_->GetRowForURL(temp_row.url(), NULL));
}

// Tests the return values from ArchiveSomeOldHistory. The rest of the
// functionality of this function is tested by the ArchiveHistoryBefore*
// tests which use this function internally.
TEST_F(ExpireHistoryTest, ArchiveSomeOldHistory) {
  URLID url_ids[3];
  Time visit_times[4];
  AddExampleData(url_ids, visit_times);

  // Deleting a time range with no URLs should return false (nothing found).
  EXPECT_FALSE(expirer_.ArchiveSomeOldHistory(
      visit_times[0] - TimeDelta::FromDays(100), 1));

  // Deleting a time range with not up the the max results should also return
  // false (there will only be one visit deleted in this range).
  EXPECT_FALSE(expirer_.ArchiveSomeOldHistory(visit_times[0], 2));

  // Deleting a time range with the max number of results should return true
  // (max deleted).
  EXPECT_TRUE(expirer_.ArchiveSomeOldHistory(visit_times[2], 1));
}

// TODO(brettw) add some visits with no URL to make sure everything is updated
// properly. Have the visits also refer to nonexistant FTS rows.
//
// Maybe also refer to invalid favicons.

}  // namespace history
