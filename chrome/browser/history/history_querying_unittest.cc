// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "chrome/browser/history/history.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;
using base::TimeDelta;

// Tests the history service for querying functionality.

namespace history {

namespace {

struct TestEntry {
  const char* url;
  const wchar_t* title;
  const int days_ago;
  const wchar_t* body;
  Time time;  // Filled by SetUp.
} test_entries[] = {
  // This one is visited super long ago so it will be in a different database
  // from the next appearance of it at the end.
  {"http://example.com/", L"Other", 180, L"Other"},

  // These are deliberately added out of chronological order. The history
  // service should sort them by visit time when returning query results.
  // The correct index sort order is 4 2 3 1 0.
  {"http://www.google.com/1", L"Title 1", 10,
   L"PAGEONE FOO some body text"},
  {"http://www.google.com/3", L"Title 3", 8,
   L"PAGETHREE BAR some hello world for you"},
  {"http://www.google.com/2", L"Title 2", 9,
   L"PAGETWO FOO some more blah blah blah"},

  // A more recent visit of the first one.
  {"http://example.com/", L"Other", 6, L"Other"},
};

// Returns true if the nth result in the given results set matches. It will
// return false on a non-match or if there aren't enough results.
bool NthResultIs(const QueryResults& results,
                 int n,  // Result index to check.
                 int test_entry_index) {  // Index of test_entries to compare.
  if (static_cast<int>(results.size()) <= n)
    return false;

  const URLResult& result = results[n];

  // Check the visit time.
  if (result.visit_time() != test_entries[test_entry_index].time)
    return false;

  // Now check the URL & title.
  return result.url() == GURL(test_entries[test_entry_index].url) &&
         result.title() == std::wstring(test_entries[test_entry_index].title);
}

}  // namespace

class HistoryQueryTest : public testing::Test {
 public:
  HistoryQueryTest() {
  }

  // Acts like a synchronous call to history's QueryHistory.
  void QueryHistory(const std::wstring& text_query,
                    const QueryOptions& options,
                    QueryResults* results) {
    history_->QueryHistory(text_query, options, &consumer_,
        NewCallback(this, &HistoryQueryTest::QueryHistoryComplete));
    MessageLoop::current()->Run();  // Will go until ...Complete calls Quit.
    results->Swap(&last_query_results_);
  }

 protected:
  scoped_refptr<HistoryService> history_;

 private:
  virtual void SetUp() {
    FilePath temp_dir;
    PathService::Get(base::DIR_TEMP, &temp_dir);
    history_dir_ = temp_dir.AppendASCII("HistoryTest");
    file_util::Delete(history_dir_, true);
    file_util::CreateDirectory(history_dir_);

    history_ = new HistoryService;
    if (!history_->Init(history_dir_, NULL)) {
      history_ = NULL;  // Tests should notice this NULL ptr & fail.
      return;
    }

    // Fill the test data.
    Time now = Time::Now().LocalMidnight();
    for (size_t i = 0; i < arraysize(test_entries); i++) {
      test_entries[i].time =
          now - (test_entries[i].days_ago * TimeDelta::FromDays(1));

      // We need the ID scope and page ID so that the visit tracker can find it.
      const void* id_scope = reinterpret_cast<void*>(1);
      int32 page_id = i;
      GURL url(test_entries[i].url);

      history_->AddPage(url, test_entries[i].time, id_scope, page_id, GURL(),
                        PageTransition::LINK, HistoryService::RedirectList());
      history_->SetPageTitle(url, test_entries[i].title);
      history_->SetPageContents(url, test_entries[i].body);
    }
  }

  virtual void TearDown() {
    if (history_.get()) {
      history_->SetOnBackendDestroyTask(new MessageLoop::QuitTask);
      history_->Cleanup();
      history_ = NULL;
      MessageLoop::current()->Run();  // Wait for the other thread.
    }
    file_util::Delete(history_dir_, true);
  }

  void QueryHistoryComplete(HistoryService::Handle, QueryResults* results) {
    results->Swap(&last_query_results_);
    MessageLoop::current()->Quit();  // Will return out to QueryHistory.
  }

  MessageLoop message_loop_;

  FilePath history_dir_;

  CancelableRequestConsumer consumer_;

  // The QueryHistoryComplete callback will put the results here so QueryHistory
  // can return them.
  QueryResults last_query_results_;

  DISALLOW_EVIL_CONSTRUCTORS(HistoryQueryTest);
};

TEST_F(HistoryQueryTest, Basic) {
  ASSERT_TRUE(history_.get());

  QueryOptions options;
  QueryResults results;

  // First query for all of them to make sure they are there and in
  // chronological order, most recent first.
  QueryHistory(std::wstring(), options, &results);
  ASSERT_EQ(5U, results.size());
  EXPECT_TRUE(NthResultIs(results, 0, 4));
  EXPECT_TRUE(NthResultIs(results, 1, 2));
  EXPECT_TRUE(NthResultIs(results, 2, 3));
  EXPECT_TRUE(NthResultIs(results, 3, 1));
  EXPECT_TRUE(NthResultIs(results, 4, 0));

  // Next query a time range. The beginning should be inclusive, the ending
  // should be exclusive.
  options.begin_time = test_entries[3].time;
  options.end_time = test_entries[2].time;
  QueryHistory(std::wstring(), options, &results);
  EXPECT_EQ(1U, results.size());
  EXPECT_TRUE(NthResultIs(results, 0, 3));
}

// Tests max_count feature for basic (non-Full Text Search) queries.
TEST_F(HistoryQueryTest, BasicCount) {
  ASSERT_TRUE(history_.get());

  QueryOptions options;
  QueryResults results;

  // Query all time but with a limit on the number of entries. We should
  // get the N most recent entries.
  options.max_count = 2;
  QueryHistory(std::wstring(), options, &results);
  EXPECT_EQ(2U, results.size());
  EXPECT_TRUE(NthResultIs(results, 0, 4));
  EXPECT_TRUE(NthResultIs(results, 1, 2));
}

// Tests duplicate collapsing and not in non-Full Text Search situations.
TEST_F(HistoryQueryTest, BasicDupes) {
  ASSERT_TRUE(history_.get());

  QueryOptions options;
  QueryResults results;

  // We did the query for no collapsing in the "Basic" test above, so here we
  // only test collapsing.
  options.most_recent_visit_only = true;
  QueryHistory(std::wstring(), options, &results);
  EXPECT_EQ(4U, results.size());
  EXPECT_TRUE(NthResultIs(results, 0, 4));
  EXPECT_TRUE(NthResultIs(results, 1, 2));
  EXPECT_TRUE(NthResultIs(results, 2, 3));
  EXPECT_TRUE(NthResultIs(results, 3, 1));
}

TEST_F(HistoryQueryTest, ReachedBeginning) {
  ASSERT_TRUE(history_.get());

  QueryOptions options;
  QueryResults results;

  QueryHistory(std::wstring(), options, &results);
  EXPECT_TRUE(results.reached_beginning());

  options.begin_time = test_entries[1].time;
  QueryHistory(std::wstring(), options, &results);
  EXPECT_FALSE(results.reached_beginning());

  options.begin_time = test_entries[0].time + TimeDelta::FromMicroseconds(1);
  QueryHistory(std::wstring(), options, &results);
  EXPECT_FALSE(results.reached_beginning());

  options.begin_time = test_entries[0].time;
  QueryHistory(std::wstring(), options, &results);
  EXPECT_TRUE(results.reached_beginning());

  options.begin_time = test_entries[0].time - TimeDelta::FromMicroseconds(1);
  QueryHistory(std::wstring(), options, &results);
  EXPECT_TRUE(results.reached_beginning());
}

// This does most of the same tests above, but searches for a FTS string that
// will match the pages in question. This will trigger a different code path.
TEST_F(HistoryQueryTest, FTS) {
  ASSERT_TRUE(history_.get());

  QueryOptions options;
  QueryResults results;

  // Query all of them to make sure they are there and in order. Note that
  // this query will return the starred item twice since we requested all
  // starred entries and no de-duping.
  QueryHistory(std::wstring(L"some"), options, &results);
  EXPECT_EQ(3U, results.size());
  EXPECT_TRUE(NthResultIs(results, 0, 2));
  EXPECT_TRUE(NthResultIs(results, 1, 3));
  EXPECT_TRUE(NthResultIs(results, 2, 1));

  // Do a query that should only match one of them.
  QueryHistory(std::wstring(L"PAGETWO"), options, &results);
  EXPECT_EQ(1U, results.size());
  EXPECT_TRUE(NthResultIs(results, 0, 3));

  // Next query a time range. The beginning should be inclusive, the ending
  // should be exclusive.
  options.begin_time = test_entries[1].time;
  options.end_time = test_entries[3].time;
  QueryHistory(std::wstring(L"some"), options, &results);
  EXPECT_EQ(1U, results.size());
  EXPECT_TRUE(NthResultIs(results, 0, 1));
}

// Searches titles.
TEST_F(HistoryQueryTest, FTSTitle) {
  ASSERT_TRUE(history_.get());

  QueryOptions options;
  QueryResults results;

  // Query all time but with a limit on the number of entries. We should
  // get the N most recent entries.
  QueryHistory(std::wstring(L"title"), options, &results);
  EXPECT_EQ(3U, results.size());
  EXPECT_TRUE(NthResultIs(results, 0, 2));
  EXPECT_TRUE(NthResultIs(results, 1, 3));
  EXPECT_TRUE(NthResultIs(results, 2, 1));
}

// Tests prefix searching for Full Text Search queries.
TEST_F(HistoryQueryTest, FTSPrefix) {
  ASSERT_TRUE(history_.get());

  QueryOptions options;
  QueryResults results;

  // Query with a prefix search.  Should return matches for "PAGETWO" and
  // "PAGETHREE".
  QueryHistory(std::wstring(L"PAGET"), options, &results);
  EXPECT_EQ(2U, results.size());
  EXPECT_TRUE(NthResultIs(results, 0, 2));
  EXPECT_TRUE(NthResultIs(results, 1, 3));
}

// Tests max_count feature for Full Text Search queries.
TEST_F(HistoryQueryTest, FTSCount) {
  ASSERT_TRUE(history_.get());

  QueryOptions options;
  QueryResults results;

  // Query all time but with a limit on the number of entries. We should
  // get the N most recent entries.
  options.max_count = 2;
  QueryHistory(std::wstring(L"some"), options, &results);
  EXPECT_EQ(2U, results.size());
  EXPECT_TRUE(NthResultIs(results, 0, 2));
  EXPECT_TRUE(NthResultIs(results, 1, 3));

  // Now query a subset of the pages and limit by N items. "FOO" should match
  // the 2nd & 3rd pages, but we should only get the 3rd one because of the one
  // page max restriction.
  options.max_count = 1;
  QueryHistory(std::wstring(L"FOO"), options, &results);
  EXPECT_EQ(1U, results.size());
  EXPECT_TRUE(NthResultIs(results, 0, 3));
}

// Tests that FTS queries can find URLs when they exist only in the archived
// database. This also tests that imported URLs can be found, since we use
// AddPageWithDetails just like the importer.
TEST_F(HistoryQueryTest, FTSArchived) {
  ASSERT_TRUE(history_.get());

  std::vector<URLRow> urls_to_add;

  URLRow row1(GURL("http://foo.bar/"));
  row1.set_title(L"archived title");
  row1.set_last_visit(Time::Now() - TimeDelta::FromDays(365));
  urls_to_add.push_back(row1);

  URLRow row2(GURL("http://foo.bar/"));
  row2.set_title(L"nonarchived title");
  row2.set_last_visit(Time::Now());
  urls_to_add.push_back(row2);

  history_->AddPagesWithDetails(urls_to_add);

  QueryOptions options;
  QueryResults results;

  // Query all time. The title we get should be the one in the full text
  // database and not the most current title (since otherwise highlighting in
  // the title might be wrong).
  QueryHistory(std::wstring(L"archived"), options, &results);
  ASSERT_EQ(1U, results.size());
  EXPECT_TRUE(row1.url() == results[0].url());
  EXPECT_TRUE(row1.title() == results[0].title());
}

/* TODO(brettw) re-enable this. It is commented out because the current history
   code prohibits adding more than one indexed page with the same URL. When we
   have tiered history, there could be a dupe in the archived history which
   won't get picked up by the deletor and it can happen again. When this is the
   case, we should fix this test to duplicate that situation.

// Tests duplicate collapsing and not in Full Text Search situations.
TEST_F(HistoryQueryTest, FTSDupes) {
  ASSERT_TRUE(history_.get());

  QueryOptions options;
  QueryResults results;

  // First do the search with collapsing.
  QueryHistory(std::wstring(L"Other"), options, &results);
  EXPECT_EQ(2, results.urls().size());
  EXPECT_TRUE(NthResultIs(results, 0, 4));
  EXPECT_TRUE(NthResultIs(results, 1, 0));

  // Now with collapsing.
  options.most_recent_visit_only = true;
  QueryHistory(std::wstring(L"Other"), options, &results);
  EXPECT_EQ(1, results.urls().size());
  EXPECT_TRUE(NthResultIs(results, 0, 4));
}
*/

}  // namespace history
