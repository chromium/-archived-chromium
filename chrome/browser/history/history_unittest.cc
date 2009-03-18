// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// History unit tests come in two flavors:
//
// 1. The more complicated style is that the unit test creates a full history
//    service. This spawns a background thread for the history backend, and
//    all communication is asynchronous. This is useful for testing more
//    complicated things or end-to-end behavior.
//
// 2. The simpler style is to create a history backend on this thread and
//    access it directly without a HistoryService object. This is much simpler
//    because communication is synchronous. Generally, sets should go through
//    the history backend (since there is a lot of logic) but gets can come
//    directly from the HistoryDatabase. This is because the backend generally
//    has no logic in the getter except threading stuff, which we don't want
//    to run.

#include <time.h>
#include <algorithm>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/gfx/jpeg_codec.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/task.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/history/history_backend.h"
#include "chrome/browser/history/history_database.h"
#include "chrome/browser/history/in_memory_database.h"
#include "chrome/browser/history/in_memory_history_backend.h"
#include "chrome/browser/history/page_usage_data.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/sqlite_utils.h"
#include "chrome/common/scoped_vector.h"
#include "chrome/common/thumbnail_score.h"
#include "chrome/tools/profiles/thumbnail-inl.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;
using base::TimeDelta;

namespace history {
class HistoryTest;
}

// Specialize RunnableMethodTraits for HistoryTest so we can create callbacks.
// None of these callbacks can outlast the test, so there is not need to retain
// the HistoryTest object.
template <>
struct RunnableMethodTraits<history::HistoryTest> {
  static void RetainCallee(history::HistoryTest* obj) { }
  static void ReleaseCallee(history::HistoryTest* obj) { }
};

namespace history {

namespace {

// Compares the two data values. Used for comparing thumbnail data.
bool DataEqual(const unsigned char* reference, size_t reference_len,
               const std::vector<unsigned char>& data) {
  if (reference_len != data.size())
    return false;
  for (size_t i = 0; i < reference_len; i++) {
    if (data[i] != reference[i])
      return false;
  }
  return true;
}

// The tracker uses RenderProcessHost pointers for scoping but never
// dereferences them. We use ints because it's easier. This function converts
// between the two.
static void* MakeFakeHost(int id) {
  void* host = 0;
  memcpy(&host, &id, sizeof(id));
  return host;
}

}  // namespace

// Delegate class for when we create a backend without a HistoryService.
class BackendDelegate : public HistoryBackend::Delegate {
 public:
  explicit BackendDelegate(HistoryTest* history_test)
      : history_test_(history_test) {
  }

  virtual void NotifyTooNew();
  virtual void SetInMemoryBackend(InMemoryHistoryBackend* backend);
  virtual void BroadcastNotifications(NotificationType type,
                                      HistoryDetails* details);
  virtual void DBLoaded() {}

 private:
  HistoryTest* history_test_;
};

// This must be outside the anonymous namespace for the friend statement in
// HistoryBackend to work.
class HistoryTest : public testing::Test {
 public:
  HistoryTest() : history_service_(NULL), db_(NULL) {
  }
  ~HistoryTest() {
  }

  // Thumbnail callback: we save the data and exit the message loop so the
  // unit test can read the data
  void OnThumbnailDataAvailable(
      HistoryService::Handle request_handle,
      scoped_refptr<RefCountedBytes> jpeg_data) {
    got_thumbnail_callback_ = true;
    if (jpeg_data.get()) {
      std::copy(jpeg_data->data.begin(), jpeg_data->data.end(),
                std::back_inserter(thumbnail_data_));
    }
    MessageLoop::current()->Quit();
  }

  // Creates the HistoryBackend and HistoryDatabase on the current thread,
  // assigning the values to backend_ and db_.
  void CreateBackendAndDatabase() {
    backend_ =
        new HistoryBackend(history_dir_, new BackendDelegate(this), NULL);
    backend_->Init();
    db_ = backend_->db_.get();
    DCHECK(in_mem_backend_.get()) << "Mem backend should have been set by "
        "HistoryBackend::Init";
  }

  void OnSegmentUsageAvailable(CancelableRequestProvider::Handle handle,
                               std::vector<PageUsageData*>* data) {
    page_usage_data_->swap(*data);
    MessageLoop::current()->Quit();
  }

  void OnDeleteURLsDone(CancelableRequestProvider::Handle handle) {
    MessageLoop::current()->Quit();
  }

 protected:
  friend class BackendDelegate;

  // testing::Test
  virtual void SetUp() {
    FilePath temp_dir;
    PathService::Get(base::DIR_TEMP, &temp_dir);
    history_dir_ = temp_dir.AppendASCII("HistoryTest");
    file_util::Delete(history_dir_, true);
    file_util::CreateDirectory(history_dir_);
  }

  void DeleteBackend() {
    if (backend_) {
      backend_->Closing();
      backend_ = NULL;
    }
  }

  virtual void TearDown() {
    DeleteBackend();

    if (history_service_)
      CleanupHistoryService();

    // Try to clean up the database file.
    file_util::Delete(history_dir_, true);

    // Make sure we don't have any event pending that could disrupt the next
    // test.
    MessageLoop::current()->PostTask(FROM_HERE, new MessageLoop::QuitTask);
    MessageLoop::current()->Run();
  }

  void CleanupHistoryService() {
    DCHECK(history_service_.get());

    history_service_->NotifyRenderProcessHostDestruction(0);
    history_service_->SetOnBackendDestroyTask(new MessageLoop::QuitTask);
    history_service_->Cleanup();
    history_service_ = NULL;

    // Wait for the backend class to terminate before deleting the files and
    // moving to the next test. Note: if this never terminates, somebody is
    // probably leaking a reference to the history backend, so it never calls
    // our destroy task.
    MessageLoop::current()->Run();
  }

  int64 AddDownload(int32 state, const Time& time) {
    DownloadCreateInfo download(FilePath(FILE_PATH_LITERAL("foo-path")),
                                GURL("foo-url"), time, 0, 512, state, 0);
    return db_->CreateDownload(download);
  }

  // Fills the query_url_row_ and query_url_visits_ structures with the
  // information about the given URL and returns true. If the URL was not
  // found, this will return false and those structures will not be changed.
  bool QueryURL(HistoryService* history, const GURL& url) {
    history->QueryURL(url, true, &consumer_,
                      NewCallback(this, &HistoryTest::SaveURLAndQuit));
    MessageLoop::current()->Run();  // Will be exited in SaveURLAndQuit.
    return query_url_success_;
  }

  // Callback for HistoryService::QueryURL.
  void SaveURLAndQuit(HistoryService::Handle handle,
                      bool success,
                      const URLRow* url_row,
                      VisitVector* visit_vector) {
    query_url_success_ = success;
    if (query_url_success_) {
      query_url_row_ = *url_row;
      query_url_visits_.swap(*visit_vector);
    } else {
      query_url_row_ = URLRow();
      query_url_visits_.clear();
    }
    MessageLoop::current()->Quit();
  }

  // Fills in saved_redirects_ with the redirect information for the given URL,
  // returning true on success. False means the URL was not found.
  bool QueryRedirectsFrom(HistoryService* history, const GURL& url) {
    history->QueryRedirectsFrom(url, &consumer_,
        NewCallback(this, &HistoryTest::OnRedirectQueryComplete));
    MessageLoop::current()->Run();  // Will be exited in *QueryComplete.
    return redirect_query_success_;
  }

  // Callback for QueryRedirects.
  void OnRedirectQueryComplete(HistoryService::Handle handle,
                               GURL url,
                               bool success,
                               HistoryService::RedirectList* redirects) {
    redirect_query_success_ = success;
    if (redirect_query_success_)
      saved_redirects_.swap(*redirects);
    else
      saved_redirects_.clear();
    MessageLoop::current()->Quit();
  }

  MessageLoopForUI message_loop_;

  // PageUsageData vector to test segments.
  ScopedVector<PageUsageData> page_usage_data_;

  // When non-NULL, this will be deleted on tear down and we will block until
  // the backend thread has completed. This allows tests for the history
  // service to use this feature, but other tests to ignore this.
  scoped_refptr<HistoryService> history_service_;

  // names of the database files
  FilePath history_dir_;

  // Set by the thumbnail callback when we get data, you should be sure to
  // clear this before issuing a thumbnail request.
  bool got_thumbnail_callback_;
  std::vector<unsigned char> thumbnail_data_;

  // Set by the redirect callback when we get data. You should be sure to
  // clear this before issuing a redirect request.
  HistoryService::RedirectList saved_redirects_;
  bool redirect_query_success_;

  // For history requests.
  CancelableRequestConsumer consumer_;

  // For saving URL info after a call to QueryURL
  bool query_url_success_;
  URLRow query_url_row_;
  VisitVector query_url_visits_;

  // Created via CreateBackendAndDatabase.
  scoped_refptr<HistoryBackend> backend_;
  scoped_ptr<InMemoryHistoryBackend> in_mem_backend_;
  HistoryDatabase* db_;  // Cached reference to the backend's database.
};

void BackendDelegate::NotifyTooNew() {
}

void BackendDelegate::SetInMemoryBackend(InMemoryHistoryBackend* backend) {
  // Save the in-memory backend to the history test object, this happens
  // synchronously, so we don't have to do anything fancy.
  history_test_->in_mem_backend_.reset(backend);
}

void BackendDelegate::BroadcastNotifications(NotificationType type,
                                             HistoryDetails* details) {
  // Currently, just send the notifications directly to the in-memory database.
  // We may want do do something more fancy in the future.
  Details<HistoryDetails> det(details);
  history_test_->in_mem_backend_->Observe(type,
      Source<HistoryTest>(NULL), det);

  // The backend passes ownership of the details pointer to us.
  delete details;
}

TEST_F(HistoryTest, ClearBrowsingData_Downloads) {
  CreateBackendAndDatabase();

  Time now = Time::Now();
  TimeDelta one_day = TimeDelta::FromDays(1);
  Time month_ago = now - TimeDelta::FromDays(30);

  // Initially there should be nothing in the downloads database.
  std::vector<DownloadCreateInfo> downloads;
  db_->QueryDownloads(&downloads);
  EXPECT_EQ(0U, downloads.size());

  // Keep track of these as we need to update them later during the test.
  DownloadID in_progress, removing;

  // Create one with a 0 time.
  EXPECT_NE(0, AddDownload(DownloadItem::COMPLETE, Time()));
  // Create one for now and +/- 1 day.
  EXPECT_NE(0, AddDownload(DownloadItem::COMPLETE, now - one_day));
  EXPECT_NE(0, AddDownload(DownloadItem::COMPLETE, now));
  EXPECT_NE(0, AddDownload(DownloadItem::COMPLETE, now + one_day));
  // Try the other three states.
  EXPECT_NE(0, AddDownload(DownloadItem::COMPLETE,    month_ago));
  EXPECT_NE(0, in_progress = AddDownload(DownloadItem::IN_PROGRESS, month_ago));
  EXPECT_NE(0, AddDownload(DownloadItem::CANCELLED,   month_ago));
  EXPECT_NE(0, removing = AddDownload(DownloadItem::REMOVING,    month_ago));

  // Test to see if inserts worked.
  db_->QueryDownloads(&downloads);
  EXPECT_EQ(8U, downloads.size());

  // Try removing from current timestamp. This should delete the one in the
  // future and one very recent one.
  db_->RemoveDownloadsBetween(now, Time());
  db_->QueryDownloads(&downloads);
  EXPECT_EQ(6U, downloads.size());

  // Try removing from two months ago. This should not delete items that are
  // 'in progress' or in 'removing' state.
  db_->RemoveDownloadsBetween(now - TimeDelta::FromDays(60), Time());
  db_->QueryDownloads(&downloads);
  EXPECT_EQ(3U, downloads.size());

  // Download manager converts to TimeT, which is lossy, so we do the same
  // for comparison.
  Time month_ago_lossy = Time::FromTimeT(month_ago.ToTimeT());

  // Make sure the right values remain.
  EXPECT_EQ(DownloadItem::COMPLETE, downloads[0].state);
  EXPECT_EQ(0, downloads[0].start_time.ToInternalValue());
  EXPECT_EQ(DownloadItem::IN_PROGRESS, downloads[1].state);
  EXPECT_EQ(month_ago_lossy.ToInternalValue(),
            downloads[1].start_time.ToInternalValue());
  EXPECT_EQ(DownloadItem::REMOVING, downloads[2].state);
  EXPECT_EQ(month_ago_lossy.ToInternalValue(),
            downloads[2].start_time.ToInternalValue());

  // Change state so we can delete the downloads.
  EXPECT_TRUE(db_->UpdateDownload(512, DownloadItem::COMPLETE, in_progress));
  EXPECT_TRUE(db_->UpdateDownload(512, DownloadItem::CANCELLED, removing));

  // Try removing from Time=0. This should delete all.
  db_->RemoveDownloadsBetween(Time(), Time());
  db_->QueryDownloads(&downloads);
  EXPECT_EQ(0U, downloads.size());
}

TEST_F(HistoryTest, AddPage) {
  scoped_refptr<HistoryService> history(new HistoryService);
  history_service_ = history;
  ASSERT_TRUE(history->Init(history_dir_, NULL));

  // Add the page once from a child frame.
  const GURL test_url("http://www.google.com/");
  history->AddPage(test_url, NULL, 0, GURL(),
                   PageTransition::MANUAL_SUBFRAME,
                   HistoryService::RedirectList());
  EXPECT_TRUE(QueryURL(history, test_url));
  EXPECT_EQ(1, query_url_row_.visit_count());
  EXPECT_EQ(0, query_url_row_.typed_count());
  EXPECT_TRUE(query_url_row_.hidden());  // Hidden because of child frame.

  // Add the page once from the main frame (should unhide it).
  history->AddPage(test_url, NULL, 0, GURL(), PageTransition::LINK,
                   HistoryService::RedirectList());
  EXPECT_TRUE(QueryURL(history, test_url));
  EXPECT_EQ(2, query_url_row_.visit_count());  // Added twice.
  EXPECT_EQ(0, query_url_row_.typed_count());  // Never typed.
  EXPECT_FALSE(query_url_row_.hidden());  // Because loaded in main frame.
}

TEST_F(HistoryTest, AddPageSameTimes) {
  scoped_refptr<HistoryService> history(new HistoryService);
  history_service_ = history;
  ASSERT_TRUE(history->Init(history_dir_, NULL));

  Time now = Time::Now();
  const GURL test_urls[] = {
    GURL("http://timer.first.page/"),
    GURL("http://timer.second.page/"),
    GURL("http://timer.third.page/"),
  };

  // Make sure that two pages added at the same time with no intervening
  // additions have different timestamps.
  history->AddPage(test_urls[0], now, NULL, 0, GURL(),
                   PageTransition::LINK,
                   HistoryService::RedirectList());
  EXPECT_TRUE(QueryURL(history, test_urls[0]));
  EXPECT_EQ(1, query_url_row_.visit_count());
  EXPECT_TRUE(now == query_url_row_.last_visit());  // gtest doesn't like Time

  history->AddPage(test_urls[1], now, NULL, 0, GURL(),
                   PageTransition::LINK,
                   HistoryService::RedirectList());
  EXPECT_TRUE(QueryURL(history, test_urls[1]));
  EXPECT_EQ(1, query_url_row_.visit_count());
  EXPECT_TRUE(now + TimeDelta::FromMicroseconds(1) ==
      query_url_row_.last_visit());

  // Make sure the next page, at a different time, is also correct.
  history->AddPage(test_urls[2], now + TimeDelta::FromMinutes(1),
                   NULL, 0, GURL(),
                   PageTransition::LINK,
                   HistoryService::RedirectList());
  EXPECT_TRUE(QueryURL(history, test_urls[2]));
  EXPECT_EQ(1, query_url_row_.visit_count());
  EXPECT_TRUE(now + TimeDelta::FromMinutes(1) ==
      query_url_row_.last_visit());
}

TEST_F(HistoryTest, AddRedirect) {
  scoped_refptr<HistoryService> history(new HistoryService);
  history_service_ = history;
  ASSERT_TRUE(history->Init(history_dir_, NULL));

  const char* first_sequence[] = {
    "http://first.page/",
    "http://second.page/"};
  int first_count = arraysize(first_sequence);
  HistoryService::RedirectList first_redirects;
  for (int i = 0; i < first_count; i++)
    first_redirects.push_back(GURL(first_sequence[i]));

  // Add the sequence of pages as a server with no referrer. Note that we need
  // to have a non-NULL page ID scope.
  history->AddPage(first_redirects.back(), MakeFakeHost(1), 0, GURL(),
                   PageTransition::LINK, first_redirects);

  // The first page should be added once with a link visit type (because we set
  // LINK when we added the original URL, and a referrer of nowhere (0).
  EXPECT_TRUE(QueryURL(history, first_redirects[0]));
  EXPECT_EQ(1, query_url_row_.visit_count());
  ASSERT_EQ(1U, query_url_visits_.size());
  int64 first_visit = query_url_visits_[0].visit_id;
  EXPECT_EQ(PageTransition::LINK |
            PageTransition::CHAIN_START, query_url_visits_[0].transition);
  EXPECT_EQ(0, query_url_visits_[0].referring_visit);  // No referrer.

  // The second page should be a server redirect type with a referrer of the
  // first page.
  EXPECT_TRUE(QueryURL(history, first_redirects[1]));
  EXPECT_EQ(1, query_url_row_.visit_count());
  ASSERT_EQ(1U, query_url_visits_.size());
  int64 second_visit = query_url_visits_[0].visit_id;
  EXPECT_EQ(PageTransition::SERVER_REDIRECT |
            PageTransition::CHAIN_END, query_url_visits_[0].transition);
  EXPECT_EQ(first_visit, query_url_visits_[0].referring_visit);

  // Check that the redirect finding function successfully reports it.
  saved_redirects_.clear();
  QueryRedirectsFrom(history, first_redirects[0]);
  ASSERT_EQ(1U, saved_redirects_.size());
  EXPECT_EQ(first_redirects[1], saved_redirects_[0]);

  // Now add a client redirect from that second visit to a third, client
  // redirects are tracked by the RenderView prior to updating history,
  // so we pass in a CLIENT_REDIRECT qualifier to mock that behavior.
  HistoryService::RedirectList second_redirects;
  second_redirects.push_back(first_redirects[1]);
  second_redirects.push_back(GURL("http://last.page/"));
  history->AddPage(second_redirects[1], MakeFakeHost(1), 1,
                   second_redirects[0],
                   static_cast<PageTransition::Type>(PageTransition::LINK |
                       PageTransition::CLIENT_REDIRECT),
                   second_redirects);

  // The last page (source of the client redirect) should NOT have an
  // additional visit added, because it was a client redirect (normally it
  // would). We should only have 1 left over from the first sequence.
  EXPECT_TRUE(QueryURL(history, second_redirects[0]));
  EXPECT_EQ(1, query_url_row_.visit_count());

  // The final page should be set as a client redirect from the previous visit.
  EXPECT_TRUE(QueryURL(history, second_redirects[1]));
  EXPECT_EQ(1, query_url_row_.visit_count());
  ASSERT_EQ(1U, query_url_visits_.size());
  EXPECT_EQ(PageTransition::CLIENT_REDIRECT |
            PageTransition::CHAIN_END, query_url_visits_[0].transition);
  EXPECT_EQ(second_visit, query_url_visits_[0].referring_visit);
}

TEST_F(HistoryTest, Typed) {
  scoped_refptr<HistoryService> history(new HistoryService);
  history_service_ = history;
  ASSERT_TRUE(history->Init(history_dir_, NULL));

  // Add the page once as typed.
  const GURL test_url("http://www.google.com/");
  history->AddPage(test_url, NULL, 0, GURL(), PageTransition::TYPED,
                   HistoryService::RedirectList());
  EXPECT_TRUE(QueryURL(history, test_url));

  // We should have the same typed & visit count.
  EXPECT_EQ(1, query_url_row_.visit_count());
  EXPECT_EQ(1, query_url_row_.typed_count());

  // Add the page again not typed.
  history->AddPage(test_url, NULL, 0, GURL(), PageTransition::LINK,
                   HistoryService::RedirectList());
  EXPECT_TRUE(QueryURL(history, test_url));

  // The second time should not have updated the typed count.
  EXPECT_EQ(2, query_url_row_.visit_count());
  EXPECT_EQ(1, query_url_row_.typed_count());

  // Add the page again as a generated URL.
  history->AddPage(test_url, NULL, 0, GURL(),
                   PageTransition::GENERATED, HistoryService::RedirectList());
  EXPECT_TRUE(QueryURL(history, test_url));

  // This should have worked like a link click.
  EXPECT_EQ(3, query_url_row_.visit_count());
  EXPECT_EQ(1, query_url_row_.typed_count());

  // Add the page again as a reload.
  history->AddPage(test_url, NULL, 0, GURL(),
                   PageTransition::RELOAD, HistoryService::RedirectList());
  EXPECT_TRUE(QueryURL(history, test_url));

  // This should not have incremented any visit counts.
  EXPECT_EQ(3, query_url_row_.visit_count());
  EXPECT_EQ(1, query_url_row_.typed_count());
}

TEST_F(HistoryTest, SetTitle) {
  scoped_refptr<HistoryService> history(new HistoryService);
  history_service_ = history;
  ASSERT_TRUE(history->Init(history_dir_, NULL));

  // Add a URL.
  const GURL existing_url("http://www.google.com/");
  history->AddPage(existing_url);

  // Set some title.
  const std::wstring existing_title(L"Google");
  history->SetPageTitle(existing_url, existing_title);

  // Make sure the title got set.
  EXPECT_TRUE(QueryURL(history, existing_url));
  EXPECT_EQ(existing_title, query_url_row_.title());

  // set a title on a nonexistent page
  const GURL nonexistent_url("http://news.google.com/");
  const std::wstring nonexistent_title(L"Google News");
  history->SetPageTitle(nonexistent_url, nonexistent_title);

  // Make sure nothing got written.
  EXPECT_FALSE(QueryURL(history, nonexistent_url));
  EXPECT_EQ(std::wstring(), query_url_row_.title());

  // TODO(brettw) this should also test redirects, which get the title of the
  // destination page.
}

TEST_F(HistoryTest, Segments) {
  scoped_refptr<HistoryService> history(new HistoryService);
  history_service_ = history;

  ASSERT_TRUE(history->Init(history_dir_, NULL));

  static const void* scope = static_cast<void*>(this);

  // Add a URL.
  const GURL existing_url("http://www.google.com/");
  history->AddPage(existing_url, scope, 0, GURL(),
                   PageTransition::TYPED, HistoryService::RedirectList());

  // Make sure a segment was created.
  history->QuerySegmentUsageSince(
      &consumer_, Time::Now() - TimeDelta::FromDays(1),
      NewCallback(static_cast<HistoryTest*>(this),
                  &HistoryTest::OnSegmentUsageAvailable));

  // Wait for processing.
  MessageLoop::current()->Run();

  EXPECT_EQ(page_usage_data_->size(), 1U);
  EXPECT_TRUE(page_usage_data_[0]->GetURL() == existing_url);
  EXPECT_DOUBLE_EQ(3.0, page_usage_data_[0]->GetScore());

  // Add a URL which doesn't create a segment.
  const GURL link_url("http://yahoo.com/");
  history->AddPage(link_url, scope, 0, GURL(),
                   PageTransition::LINK, HistoryService::RedirectList());

  // Query again
  history->QuerySegmentUsageSince(
      &consumer_, Time::Now() - TimeDelta::FromDays(1),
      NewCallback(static_cast<HistoryTest*>(this),
                  &HistoryTest::OnSegmentUsageAvailable));

  // Wait for processing.
  MessageLoop::current()->Run();

  // Make sure we still have one segment.
  EXPECT_EQ(page_usage_data_->size(), 1U);
  EXPECT_TRUE(page_usage_data_[0]->GetURL() == existing_url);

  // Add a page linked from existing_url.
  history->AddPage(GURL("http://www.google.com/foo"), scope, 3, existing_url,
                   PageTransition::LINK, HistoryService::RedirectList());

  // Query again
  history->QuerySegmentUsageSince(
      &consumer_, Time::Now() - TimeDelta::FromDays(1),
      NewCallback(static_cast<HistoryTest*>(this),
                  &HistoryTest::OnSegmentUsageAvailable));

  // Wait for processing.
  MessageLoop::current()->Run();

  // Make sure we still have one segment.
  EXPECT_EQ(page_usage_data_->size(), 1U);
  EXPECT_TRUE(page_usage_data_[0]->GetURL() == existing_url);

  // However, the score should have increased.
  EXPECT_GT(page_usage_data_[0]->GetScore(), 5.0);
}

// This just tests history system -> thumbnail database integration, the actual
// thumbnail tests are in its own file.
TEST_F(HistoryTest, Thumbnails) {
  scoped_refptr<HistoryService> history(new HistoryService);
  history_service_ = history;
  ASSERT_TRUE(history->Init(history_dir_, NULL));

  scoped_ptr<SkBitmap> thumbnail(
      JPEGCodec::Decode(kGoogleThumbnail, sizeof(kGoogleThumbnail)));
  static const double boringness = 0.25;

  const GURL url("http://www.google.com/thumbnail_test/");
  history->AddPage(url);  // Must be visited before adding a thumbnail.
  history->SetPageThumbnail(url, *thumbnail,
                            ThumbnailScore(boringness, true, true));

  // Make sure we get the correct thumbnail data.
  EXPECT_TRUE(history->GetPageThumbnail(url, &consumer_,
      NewCallback(static_cast<HistoryTest*>(this),
                  &HistoryTest::OnThumbnailDataAvailable)));
  thumbnail_data_.clear();
  MessageLoop::current()->Run();
  // Make sure we got a valid JPEG back. This isn't equivalent to
  // being correct, but when we're roundtripping through JPEG
  // compression and we don't have a similarity measure.
  EXPECT_TRUE(thumbnail_data_.size());
  scoped_ptr<SkBitmap> decoded_thumbnail(
      JPEGCodec::Decode(&thumbnail_data_[0], thumbnail_data_.size()));
  EXPECT_TRUE(decoded_thumbnail.get());

  // Request a nonexistent thumbnail and make sure we get
  // a callback and no data.
  EXPECT_TRUE(history->GetPageThumbnail(GURL("http://asdfasdf.com/"),
              &consumer_,
                  NewCallback(static_cast<HistoryTest*>(this),
                              &HistoryTest::OnThumbnailDataAvailable)));
  thumbnail_data_.clear();
  MessageLoop::current()->Run();
  EXPECT_EQ(0U, thumbnail_data_.size());

  // Request the thumbnail and cancel the request..
  got_thumbnail_callback_ = false;
  thumbnail_data_.clear();
  HistoryService::Handle handle = history->GetPageThumbnail(url, &consumer_,
      NewCallback(static_cast<HistoryTest*>(this),
                  &HistoryTest::OnThumbnailDataAvailable));
  EXPECT_TRUE(handle);

  history->CancelRequest(handle);

  // We create a task with a timeout so we can make sure we don't get and
  // data in that time.
  class QuitMessageLoop : public Task {
   public:
    virtual void Run() {
      MessageLoop::current()->Quit();
    }
  };
  MessageLoop::current()->PostDelayedTask(FROM_HERE, new QuitMessageLoop, 2000);
  MessageLoop::current()->Run();
  EXPECT_FALSE(got_thumbnail_callback_);
}

// The version of the history database should be current in the "typical
// history" example file or it will be imported on startup, throwing off timing
// measurements.
//
// See test/data/profiles/typical_history/README.txt for instructions on
// how to up the version.
TEST(HistoryProfileTest, TypicalProfileVersion) {
  FilePath file;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &file));
  file = file.AppendASCII("profiles");
  file = file.AppendASCII("typical_history");
  file = file.AppendASCII("Default");
  file = file.AppendASCII("History");

  int cur_version = HistoryDatabase::GetCurrentVersion();

  sqlite3* db;
  ASSERT_EQ(SQLITE_OK, OpenSqliteDb(file, &db));

  {
    SQLStatement s;
    ASSERT_EQ(SQLITE_OK, s.prepare(db,
        "SELECT value FROM meta WHERE key = 'version'"));
    EXPECT_EQ(SQLITE_ROW, s.step());
    int file_version = s.column_int(0);
    EXPECT_EQ(cur_version, file_version);
  }

  ASSERT_EQ(SQLITE_OK, sqlite3_close(db));
}

namespace {

// Use this dummy value to scope the page IDs we give history.
static const void* kAddArgsScope = (void*)0x12345678;

// Creates a new HistoryAddPageArgs object for sending to the history database
// with reasonable defaults and the given NULL-terminated URL string. The
// returned object will NOT be add-ref'ed, which is the responsibility of the
// caller.
HistoryAddPageArgs* MakeAddArgs(const GURL& url) {
  return new HistoryAddPageArgs(url,
                                Time::Now(),
                                kAddArgsScope,
                                0,
                                GURL(),
                                HistoryService::RedirectList(),
                                PageTransition::TYPED);
}

// Convenience version of the above to convert a char string.
HistoryAddPageArgs* MakeAddArgs(const char* url) {
  return MakeAddArgs(GURL(url));
}

// A HistoryDBTask implementation. Each time RunOnDBThread is invoked
// invoke_count is increment. When invoked kWantInvokeCount times, true is
// returned from RunOnDBThread which should stop RunOnDBThread from being
// invoked again. When DoneRunOnMainThread is invoked, done_invoked is set to
// true.
class HistoryDBTaskImpl : public HistoryDBTask {
 public:
  static const int kWantInvokeCount;

  HistoryDBTaskImpl() : invoke_count(0), done_invoked(false) {}
  virtual ~HistoryDBTaskImpl() {}

  virtual bool RunOnDBThread(HistoryBackend* backend, HistoryDatabase* db) {
    return (++invoke_count == kWantInvokeCount);
  }

  virtual void DoneRunOnMainThread() {
    done_invoked = true;
    MessageLoop::current()->Quit();
  }

  int invoke_count;
  bool done_invoked;

 private:
  DISALLOW_EVIL_CONSTRUCTORS(HistoryDBTaskImpl);
};

// static
const int HistoryDBTaskImpl::kWantInvokeCount = 2;

}  // namespace

TEST_F(HistoryTest, HistoryDBTask) {
  CancelableRequestConsumerT<int, 0> request_consumer;
  HistoryService* history = new HistoryService();
  ASSERT_TRUE(history->Init(history_dir_, NULL));
  scoped_refptr<HistoryDBTaskImpl> task(new HistoryDBTaskImpl());
  history_service_ = history;
  history->ScheduleDBTask(task.get(), &request_consumer);
  // Run the message loop. When HistoryDBTaskImpl::DoneRunOnMainThread runs,
  // it will stop the message loop. If the test hangs here, it means
  // DoneRunOnMainThread isn't being invoked correctly.
  MessageLoop::current()->Run();
  CleanupHistoryService();
  // WARNING: history has now been deleted.
  history = NULL;
  ASSERT_EQ(HistoryDBTaskImpl::kWantInvokeCount, task->invoke_count);
  ASSERT_TRUE(task->done_invoked);
}

TEST_F(HistoryTest, HistoryDBTaskCanceled) {
  CancelableRequestConsumerT<int, 0> request_consumer;
  HistoryService* history = new HistoryService();
  ASSERT_TRUE(history->Init(history_dir_, NULL));
  scoped_refptr<HistoryDBTaskImpl> task(new HistoryDBTaskImpl());
  history_service_ = history;
  history->ScheduleDBTask(task.get(), &request_consumer);
  request_consumer.CancelAllRequests();
  CleanupHistoryService();
  // WARNING: history has now been deleted.
  history = NULL;
  ASSERT_FALSE(task->done_invoked);
}

}  // namespace history
