// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/gfx/jpeg_codec.h"
#include "base/path_service.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/history/history_backend.h"
#include "chrome/browser/history/in_memory_history_backend.h"
#include "chrome/browser/history/in_memory_database.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/thumbnail_score.h"
#include "chrome/tools/profiles/thumbnail-inl.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;

// This file only tests functionality where it is most convenient to call the
// backend directly. Most of the history backend functions are tested by the
// history unit test. Because of the elaborate callbacks involved, this is no
// harder than calling it directly for many things.

namespace history {

class HistoryBackendTest;

// This must be a separate object since HistoryBackend manages its lifetime.
// This just forwards the messages we're interested in to the test object.
class HistoryBackendTestDelegate : public HistoryBackend::Delegate {
 public:
  explicit HistoryBackendTestDelegate(HistoryBackendTest* test) : test_(test) {
  }

  virtual void NotifyTooNew() {
  }
  virtual void SetInMemoryBackend(InMemoryHistoryBackend* backend);
  virtual void BroadcastNotifications(NotificationType type,
                                      HistoryDetails* details);
  virtual void DBLoaded();

 private:
  // Not owned by us.
  HistoryBackendTest* test_;

  DISALLOW_EVIL_CONSTRUCTORS(HistoryBackendTestDelegate);
};

class HistoryBackendTest : public testing::Test {
 public:
  HistoryBackendTest() : bookmark_model_(NULL), loaded_(false) {}
  virtual ~HistoryBackendTest() {
  }

 protected:
  scoped_refptr<HistoryBackend> backend_;  // Will be NULL on init failure.
  scoped_ptr<InMemoryHistoryBackend> mem_backend_;

  void AddRedirectChain(const char* sequence[], int page_id) {
    HistoryService::RedirectList redirects;
    for (int i = 0; sequence[i] != NULL; ++i)
      redirects.push_back(GURL(sequence[i]));

    int int_scope = 1;
    void* scope = 0;
    memcpy(&scope, &int_scope, sizeof(int_scope));
    scoped_refptr<history::HistoryAddPageArgs> request(
        new history::HistoryAddPageArgs(
            redirects.back(), Time::Now(), scope, page_id, GURL(),
            redirects, PageTransition::LINK));
    backend_->AddPage(request);
  }

  BookmarkModel bookmark_model_;

 protected:
  bool loaded_;

 private:
  friend class HistoryBackendTestDelegate;

  // testing::Test
  virtual void SetUp() {
    if (!file_util::CreateNewTempDirectory(FILE_PATH_LITERAL("BackendTest"),
                                           &test_dir_))
      return;
    backend_ = new HistoryBackend(test_dir_,
                                  new HistoryBackendTestDelegate(this),
                                  &bookmark_model_);
    backend_->Init();
  }
  virtual void TearDown() {
    backend_->Closing();
    backend_ = NULL;
    mem_backend_.reset();
    file_util::Delete(test_dir_, true);
  }

  void SetInMemoryBackend(InMemoryHistoryBackend* backend) {
    mem_backend_.reset(backend);
  }

  void BroadcastNotifications(NotificationType type,
                                      HistoryDetails* details) {
    // Send the notifications directly to the in-memory database.
    Details<HistoryDetails> det(details);
    mem_backend_->Observe(type, Source<HistoryBackendTest>(NULL), det);

    // The backend passes ownership of the details pointer to us.
    delete details;
  }

  MessageLoop message_loop_;
  FilePath test_dir_;
};

void HistoryBackendTestDelegate::SetInMemoryBackend(
    InMemoryHistoryBackend* backend) {
  test_->SetInMemoryBackend(backend);
}

void HistoryBackendTestDelegate::BroadcastNotifications(
    NotificationType type,
    HistoryDetails* details) {
  test_->BroadcastNotifications(type, details);
}

void HistoryBackendTestDelegate::DBLoaded() {
  test_->loaded_ = true;
}

TEST_F(HistoryBackendTest, Loaded) {
  ASSERT_TRUE(backend_.get());
  ASSERT_TRUE(loaded_);
}

TEST_F(HistoryBackendTest, DeleteAll) {
  ASSERT_TRUE(backend_.get());

  // Add two favicons, use the characters '1' and '2' for the image data. Note
  // that we do these in the opposite order. This is so the first one gets ID
  // 2 autoassigned to the database, which will change when the other one is
  // deleted. This way we can test that updating works properly.
  GURL favicon_url1("http://www.google.com/favicon.ico");
  GURL favicon_url2("http://news.google.com/favicon.ico");
  FavIconID favicon2 = backend_->thumbnail_db_->AddFavIcon(favicon_url2);
  FavIconID favicon1 = backend_->thumbnail_db_->AddFavIcon(favicon_url1);

  std::vector<unsigned char> data;
  data.push_back('1');
  EXPECT_TRUE(backend_->thumbnail_db_->SetFavIcon(
                  favicon1, data, Time::Now()));

  data[0] = '2';
  EXPECT_TRUE(backend_->thumbnail_db_->SetFavIcon(
                  favicon2, data, Time::Now()));

  // First visit two URLs.
  URLRow row1(GURL("http://www.google.com/"));
  row1.set_visit_count(2);
  row1.set_typed_count(1);
  row1.set_last_visit(Time::Now());
  row1.set_favicon_id(favicon1);

  URLRow row2(GURL("http://news.google.com/"));
  row2.set_visit_count(1);
  row2.set_last_visit(Time::Now());
  row2.set_favicon_id(favicon2);

  std::vector<URLRow> rows;
  rows.push_back(row2);  // Reversed order for the same reason as favicons.
  rows.push_back(row1);
  backend_->AddPagesWithDetails(rows);

  URLID row1_id = backend_->db_->GetRowForURL(row1.url(), NULL);
  URLID row2_id = backend_->db_->GetRowForURL(row2.url(), NULL);

  // Get the two visits for the URLs we just added.
  VisitVector visits;
  backend_->db_->GetVisitsForURL(row1_id, &visits);
  ASSERT_EQ(1U, visits.size());
  VisitID visit1_id = visits[0].visit_id;

  visits.clear();
  backend_->db_->GetVisitsForURL(row2_id, &visits);
  ASSERT_EQ(1U, visits.size());
  VisitID visit2_id = visits[0].visit_id;

  // The in-memory backend should have been set and it should have gotten the
  // typed URL.
  ASSERT_TRUE(mem_backend_.get());
  URLRow outrow1;
  EXPECT_TRUE(mem_backend_->db_->GetRowForURL(row1.url(), NULL));

  // Add thumbnails for each page.
  ThumbnailScore score(0.25, true, true);
  scoped_ptr<SkBitmap> google_bitmap(
      JPEGCodec::Decode(kGoogleThumbnail, sizeof(kGoogleThumbnail)));

  Time time;
  GURL gurl;
  backend_->thumbnail_db_->SetPageThumbnail(gurl, row1_id, *google_bitmap,
                                            score, time);
  scoped_ptr<SkBitmap> weewar_bitmap(
     JPEGCodec::Decode(kWeewarThumbnail, sizeof(kWeewarThumbnail)));
  backend_->thumbnail_db_->SetPageThumbnail(gurl, row2_id, *weewar_bitmap,
                                            score, time);

  // Star row1.
  bookmark_model_.AddURL(
      bookmark_model_.GetBookmarkBarNode(), 0, std::wstring(), row1.url());

  // Set full text index for each one.
  backend_->text_database_->AddPageData(row1.url(), row1_id, visit1_id,
                                        row1.last_visit(),
                                        L"Title 1", L"Body 1");
  backend_->text_database_->AddPageData(row2.url(), row2_id, visit2_id,
                                        row2.last_visit(),
                                        L"Title 2", L"Body 2");

  // Now finally clear all history.
  backend_->DeleteAllHistory();

  // The first URL should be preserved but the time should be cleared.
  EXPECT_TRUE(backend_->db_->GetRowForURL(row1.url(), &outrow1));
  EXPECT_EQ(0, outrow1.visit_count());
  EXPECT_EQ(0, outrow1.typed_count());
  EXPECT_TRUE(Time() == outrow1.last_visit());

  // The second row should be deleted.
  URLRow outrow2;
  EXPECT_FALSE(backend_->db_->GetRowForURL(row2.url(), &outrow2));

  // All visits should be deleted for both URLs.
  VisitVector all_visits;
  backend_->db_->GetAllVisitsInRange(Time(), Time(), 0, &all_visits);
  ASSERT_EQ(0U, all_visits.size());

  // All thumbnails should be deleted.
  std::vector<unsigned char> out_data;
  EXPECT_FALSE(backend_->thumbnail_db_->GetPageThumbnail(outrow1.id(),
                                                         &out_data));
  EXPECT_FALSE(backend_->thumbnail_db_->GetPageThumbnail(row2_id, &out_data));

  // We should have a favicon for the first URL only. We look them up by favicon
  // URL since the IDs may hav changed.
  FavIconID out_favicon1 = backend_->thumbnail_db_->
      GetFavIconIDForFavIconURL(favicon_url1);
  EXPECT_TRUE(out_favicon1);
  FavIconID out_favicon2 = backend_->thumbnail_db_->
      GetFavIconIDForFavIconURL(favicon_url2);
  EXPECT_FALSE(out_favicon2) << "Favicon not deleted";

  // The remaining URL should still reference the same favicon, even if its
  // ID has changed.
  EXPECT_EQ(out_favicon1, outrow1.favicon_id());

  // The first URL should still be bookmarked.
  EXPECT_TRUE(bookmark_model_.IsBookmarked(row1.url()));

  // The full text database should have no data.
  std::vector<TextDatabase::Match> text_matches;
  Time first_time_searched;
  backend_->text_database_->GetTextMatches(L"Body", QueryOptions(),
                                           &text_matches,
                                           &first_time_searched);
  EXPECT_EQ(0U, text_matches.size());
}

TEST_F(HistoryBackendTest, URLsNoLongerBookmarked) {
  GURL favicon_url1("http://www.google.com/favicon.ico");
  GURL favicon_url2("http://news.google.com/favicon.ico");
  FavIconID favicon2 = backend_->thumbnail_db_->AddFavIcon(favicon_url2);
  FavIconID favicon1 = backend_->thumbnail_db_->AddFavIcon(favicon_url1);

  std::vector<unsigned char> data;
  data.push_back('1');
  EXPECT_TRUE(backend_->thumbnail_db_->SetFavIcon(
                  favicon1, data, Time::Now()));

  data[0] = '2';
  EXPECT_TRUE(backend_->thumbnail_db_->SetFavIcon(
                  favicon2, data, Time::Now()));

  // First visit two URLs.
  URLRow row1(GURL("http://www.google.com/"));
  row1.set_visit_count(2);
  row1.set_typed_count(1);
  row1.set_last_visit(Time::Now());
  row1.set_favicon_id(favicon1);

  URLRow row2(GURL("http://news.google.com/"));
  row2.set_visit_count(1);
  row2.set_last_visit(Time::Now());
  row2.set_favicon_id(favicon2);

  std::vector<URLRow> rows;
  rows.push_back(row2);  // Reversed order for the same reason as favicons.
  rows.push_back(row1);
  backend_->AddPagesWithDetails(rows);

  URLID row1_id = backend_->db_->GetRowForURL(row1.url(), NULL);
  URLID row2_id = backend_->db_->GetRowForURL(row2.url(), NULL);

  // Star the two URLs.
  bookmark_model_.SetURLStarred(row1.url(), std::wstring(), true);
  bookmark_model_.SetURLStarred(row2.url(), std::wstring(), true);

  // Delete url 2. Because url 2 is starred this won't delete the URL, only
  // the visits.
  backend_->expirer_.DeleteURL(row2.url());

  // Make sure url 2 is still valid, but has no visits.
  URLRow tmp_url_row;
  EXPECT_EQ(row2_id, backend_->db_->GetRowForURL(row2.url(), NULL));
  VisitVector visits;
  backend_->db_->GetVisitsForURL(row2_id, &visits);
  EXPECT_EQ(0U, visits.size());
  // The favicon should still be valid.
  EXPECT_EQ(favicon2,
      backend_->thumbnail_db_->GetFavIconIDForFavIconURL(favicon_url2));

  // Unstar row2.
  bookmark_model_.SetURLStarred(row2.url(), std::wstring(), false);
  // Tell the backend it was unstarred. We have to explicitly do this as
  // BookmarkModel isn't wired up to the backend during testing.
  std::set<GURL> unstarred_urls;
  unstarred_urls.insert(row2.url());
  backend_->URLsNoLongerBookmarked(unstarred_urls);

  // The URL should no longer exist.
  EXPECT_FALSE(backend_->db_->GetRowForURL(row2.url(), &tmp_url_row));
  // And the favicon should be deleted.
  EXPECT_EQ(0,
      backend_->thumbnail_db_->GetFavIconIDForFavIconURL(favicon_url2));

  // Unstar row 1.
  bookmark_model_.SetURLStarred(row1.url(), std::wstring(), false);
  // Tell the backend it was unstarred. We have to explicitly do this as
  // BookmarkModel isn't wired up to the backend during testing.
  unstarred_urls.clear();
  unstarred_urls.insert(row1.url());
  backend_->URLsNoLongerBookmarked(unstarred_urls);

  // The URL should still exist (because there were visits).
  EXPECT_EQ(row1_id, backend_->db_->GetRowForURL(row1.url(), NULL));

  // There should still be visits.
  visits.clear();
  backend_->db_->GetVisitsForURL(row1_id, &visits);
  EXPECT_EQ(1U, visits.size());

  // The favicon should still be valid.
  EXPECT_EQ(favicon1,
      backend_->thumbnail_db_->GetFavIconIDForFavIconURL(favicon_url1));
}

TEST_F(HistoryBackendTest, GetPageThumbnailAfterRedirects) {
  ASSERT_TRUE(backend_.get());

  const char* base_url = "http://mail";
  const char* thumbnail_url = "http://mail.google.com";
  const char* first_chain[] = {
    base_url,
    thumbnail_url,
    NULL
  };
  AddRedirectChain(first_chain, 0);

  // Add a thumbnail for the end of that redirect chain.
  scoped_ptr<SkBitmap> thumbnail(
      JPEGCodec::Decode(kGoogleThumbnail, sizeof(kGoogleThumbnail)));
  backend_->SetPageThumbnail(GURL(thumbnail_url), *thumbnail,
                             ThumbnailScore(0.25, true, true));

  // Write a second URL chain so that if you were to simply check what
  // "http://mail" redirects to, you wouldn't see the URL that has
  // contains the thumbnail.
  const char* second_chain[] = {
    base_url,
    "http://mail.google.com/somewhere/else",
    NULL
  };
  AddRedirectChain(second_chain, 1);

  // Now try to get the thumbnail for the base url. It shouldn't be
  // distracted by the second chain and should return the thumbnail
  // attached to thumbnail_url_.
  scoped_refptr<RefCountedBytes> data;
  backend_->GetPageThumbnailDirectly(GURL(base_url), &data);

  EXPECT_TRUE(data.get());
}

}  // namespace history
