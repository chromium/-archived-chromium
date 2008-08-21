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

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/history/history_backend.h"
#include "chrome/browser/history/in_memory_history_backend.h"
#include "chrome/browser/history/in_memory_database.h"
#include "chrome/common/jpeg_codec.h"
#include "chrome/common/thumbnail_score.h"
#include "chrome/tools/profiles/thumbnail-inl.h"
#include "testing/gtest/include/gtest/gtest.h"

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

 private:
  // Not owned by us.
  HistoryBackendTest* test_;

  DISALLOW_EVIL_CONSTRUCTORS(HistoryBackendTestDelegate);
};

class HistoryBackendTest : public testing::Test {
 public:
  virtual ~HistoryBackendTest() {
  }

 protected:
  scoped_refptr<HistoryBackend> backend_;  // Will be NULL on init failure.
  scoped_ptr<InMemoryHistoryBackend> mem_backend_;

  void AddRedirectChain(const wchar_t* sequence[], int page_id) {
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

 private:
  friend HistoryBackendTestDelegate;

  // testing::Test
  virtual void SetUp() {
    if (!file_util::CreateNewTempDirectory(L"BackendTest", &test_dir_))
      return;
    backend_ = new HistoryBackend(test_dir_,
                                  new HistoryBackendTestDelegate(this));
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
    mem_backend_->Observe(type, Source<HistoryTest>(NULL), det);

    // The backend passes ownership of the details pointer to us.
    delete details;
  }

  std::wstring test_dir_;
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
  ASSERT_EQ(1, visits.size());
  VisitID visit1_id = visits[0].visit_id;

  visits.clear();
  backend_->db_->GetVisitsForURL(row2_id, &visits);
  ASSERT_EQ(1, visits.size());
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
  backend_->thumbnail_db_->SetPageThumbnail(row1_id, *google_bitmap, score);
  scoped_ptr<SkBitmap> weewar_bitmap(
     JPEGCodec::Decode(kWeewarThumbnail, sizeof(kWeewarThumbnail)));
  backend_->thumbnail_db_->SetPageThumbnail(row2_id, *weewar_bitmap, score);

  // Set full text index for each one.
  backend_->text_database_->AddPageData(row1.url(), row1_id, visit1_id,
                                        row1.last_visit(),
                                        L"Title 1", L"Body 1");
  backend_->text_database_->AddPageData(row2.url(), row2_id, visit2_id,
                                        row2.last_visit(),
                                        L"Title 2", L"Body 2");

  // Now finally clear all history.
  backend_->DeleteAllHistory();

  // The first URL should be deleted.
  EXPECT_FALSE(backend_->db_->GetRowForURL(row1.url(), &outrow1));

  // The second row should be deleted.
  URLRow outrow2;
  EXPECT_FALSE(backend_->db_->GetRowForURL(row2.url(), &outrow2));

  // All visits should be deleted for both URLs.
  VisitVector all_visits;
  backend_->db_->GetAllVisitsInRange(Time(), Time(), 0, &all_visits);
  ASSERT_EQ(0, all_visits.size());

  // All thumbnails should be deleted.
  std::vector<unsigned char> out_data;
  EXPECT_FALSE(backend_->thumbnail_db_->GetPageThumbnail(outrow1.id(),
                                                         &out_data));
  EXPECT_FALSE(backend_->thumbnail_db_->GetPageThumbnail(row2_id, &out_data));

  // Make sure the favicons were deleted.
  // TODO(sky): would be nice if this didn't happen.
  FavIconID out_favicon1 = backend_->thumbnail_db_->
      GetFavIconIDForFavIconURL(favicon_url1);
  EXPECT_FALSE(out_favicon1);
  FavIconID out_favicon2 = backend_->thumbnail_db_->
      GetFavIconIDForFavIconURL(favicon_url2);
  EXPECT_FALSE(out_favicon2) << "Favicon not deleted";

  // The full text database should have no data.
  std::vector<TextDatabase::Match> text_matches;
  Time first_time_searched;
  backend_->text_database_->GetTextMatches(L"Body", QueryOptions(),
                                           &text_matches,
                                           &first_time_searched);
  EXPECT_EQ(0, text_matches.size());
}

TEST_F(HistoryBackendTest, GetPageThumbnailAfterRedirects) {
  ASSERT_TRUE(backend_.get());

  const wchar_t* base_url = L"http://mail";
  const wchar_t* thumbnail_url = L"http://mail.google.com";
  const wchar_t* first_chain[] = {
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
  const wchar_t* second_chain[] = {
    base_url,
    L"http://mail.google.com/somewhere/else",
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
