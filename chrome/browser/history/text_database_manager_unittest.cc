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
#include "chrome/browser/history/text_database_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace history {

namespace {

const char* kURL1 = "http://www.google.com/asdf";
const wchar_t* kTitle1 = L"Google A";
const wchar_t* kBody1 = L"FOO page one.";

const char* kURL2 = "http://www.google.com/qwer";
const wchar_t* kTitle2 = L"Google B";
const wchar_t* kBody2 = L"FOO two.";

const char* kURL3 = "http://www.google.com/zxcv";
const wchar_t* kTitle3 = L"Google C";
const wchar_t* kBody3 = L"FOO drei";

const char* kURL4 = "http://www.google.com/hjkl";
const wchar_t* kTitle4 = L"Google D";
const wchar_t* kBody4 = L"FOO lalala four.";

const char* kURL5 = "http://www.google.com/uiop";
const wchar_t* kTitle5 = L"Google cinq";
const wchar_t* kBody5 = L"FOO page one.";

class TextDatabaseManagerTest : public testing::Test {
 public:
  // Called manually by the test so it can report failure to initialize.
  bool Init() {
    return file_util::CreateNewTempDirectory(L"TestSearchTest", &dir_);
  }

 protected:
  void SetUp() {
  }

  void TearDown() {
    file_util::Delete(dir_, true);
  }

  // Directory containing the databases.
  std::wstring dir_;
};

// This provides a simple implementation of a VisitDatabase using an in-memory
// sqlite connection. The text database manager expects to be able to update
// the visit database to keep in sync.
class InMemVisitDB : public VisitDatabase {
 public:
  InMemVisitDB() {
    sqlite3_open(":memory:", &db_);
    statement_cache_ = new SqliteStatementCache(db_);
    InitVisitTable();
  }
  ~InMemVisitDB() {
    delete statement_cache_;
    sqlite3_close(db_);
  }

 private:
  virtual sqlite3* GetDB() { return db_; }
  virtual SqliteStatementCache& GetStatementCache() {
    return *statement_cache_;
  }

  sqlite3* db_;
  SqliteStatementCache* statement_cache_;

  DISALLOW_EVIL_CONSTRUCTORS(InMemVisitDB);
};

// Adds all the pages once, and the first page once more in the next month.
// The times of all the pages will be filled into |*times|.
void AddAllPages(TextDatabaseManager& manager, VisitDatabase* visit_db,
                 std::vector<Time>* times) {
  Time::Exploded exploded;
  memset(&exploded, 0, sizeof(Time::Exploded));

  // Put the visits in two different months so it will query across databases.
  exploded.year = 2008;
  exploded.month = 1;
  exploded.day_of_month = 3;

  VisitRow visit_row;
  visit_row.url_id = 1;
  visit_row.visit_time = Time::FromUTCExploded(exploded);
  visit_row.referring_visit = 0;
  visit_row.transition = 0;
  visit_row.segment_id = 0;
  visit_row.is_indexed = false;
  VisitID visit_id = visit_db->AddVisit(&visit_row);

  times->push_back(visit_row.visit_time);
  manager.AddPageData(GURL(kURL1), visit_row.url_id, visit_row.visit_id,
                      visit_row.visit_time, kTitle1, kBody1);

  exploded.day_of_month++;
  visit_row.url_id = 2;
  visit_row.visit_time = Time::FromUTCExploded(exploded);
  visit_id = visit_db->AddVisit(&visit_row);
  times->push_back(visit_row.visit_time);
  manager.AddPageData(GURL(kURL2), visit_row.url_id, visit_row.visit_id,
                      visit_row.visit_time, kTitle2, kBody2);

  exploded.day_of_month++;
  visit_row.url_id = 2;
  visit_row.visit_time = Time::FromUTCExploded(exploded);
  visit_id = visit_db->AddVisit(&visit_row);
  times->push_back(visit_row.visit_time);
  manager.AddPageData(GURL(kURL3), visit_row.url_id, visit_row.visit_id,
                      visit_row.visit_time, kTitle3, kBody3);

  // Put the next ones in the next month.
  exploded.month++;
  visit_row.url_id = 2;
  visit_row.visit_time = Time::FromUTCExploded(exploded);
  visit_id = visit_db->AddVisit(&visit_row);
  times->push_back(visit_row.visit_time);
  manager.AddPageData(GURL(kURL4), visit_row.url_id, visit_row.visit_id,
                      visit_row.visit_time, kTitle4, kBody4);

  exploded.day_of_month++;
  visit_row.url_id = 2;
  visit_row.visit_time = Time::FromUTCExploded(exploded);
  visit_id = visit_db->AddVisit(&visit_row);
  times->push_back(visit_row.visit_time);
  manager.AddPageData(GURL(kURL5), visit_row.url_id, visit_row.visit_id,
                      visit_row.visit_time, kTitle5, kBody5);

  // Put the first one in again in the second month.
  exploded.day_of_month++;
  visit_row.url_id = 2;
  visit_row.visit_time = Time::FromUTCExploded(exploded);
  visit_id = visit_db->AddVisit(&visit_row);
  times->push_back(visit_row.visit_time);
  manager.AddPageData(GURL(kURL1), visit_row.url_id, visit_row.visit_id,
                      visit_row.visit_time, kTitle1, kBody1);
}

bool ResultsHaveURL(const std::vector<TextDatabase::Match>& results,
                    const char* url) {
  GURL gurl(url);
  for (size_t i = 0; i < results.size(); i++) {
    if (results[i].url == gurl)
      return true;
  }
  return false;
}

}  // namespace

// Tests basic querying.
TEST_F(TextDatabaseManagerTest, InsertQuery) {
  ASSERT_TRUE(Init());
  InMemVisitDB visit_db;
  TextDatabaseManager manager(dir_, &visit_db);
  ASSERT_TRUE(manager.Init());

  std::vector<Time> times;
  AddAllPages(manager, &visit_db, &times);

  QueryOptions options;
  options.begin_time = times[0] - TimeDelta::FromDays(100);
  options.end_time = times[times.size() - 1] + TimeDelta::FromDays(100);
  std::vector<TextDatabase::Match> results;
  Time first_time_searched;
  manager.GetTextMatches(L"FOO", options, &results, &first_time_searched);

  // We should have matched every page.
  EXPECT_EQ(6, results.size());
  EXPECT_TRUE(ResultsHaveURL(results, kURL1));
  EXPECT_TRUE(ResultsHaveURL(results, kURL2));
  EXPECT_TRUE(ResultsHaveURL(results, kURL3));
  EXPECT_TRUE(ResultsHaveURL(results, kURL4));
  EXPECT_TRUE(ResultsHaveURL(results, kURL5));

  // The first time searched should have been the first page's time or before
  // (it could have eliminated some time for us).
  EXPECT_TRUE(first_time_searched <= times[0]);
}

// Tests that adding page components piecemeal will get them added properly.
// This does not supply a visit to update, this mode is used only by the unit
// tests right now, but we test it anyway.
TEST_F(TextDatabaseManagerTest, InsertCompleteNoVisit) {
  ASSERT_TRUE(Init());
  InMemVisitDB visit_db;
  TextDatabaseManager manager(dir_, &visit_db);
  ASSERT_TRUE(manager.Init());

  // First add one without a visit.
  const GURL url(kURL1);
  manager.AddPageURL(url, 0, 0, Time::Now());
  manager.AddPageTitle(url, kTitle1);
  manager.AddPageContents(url, kBody1);

  // Check that the page got added.
  QueryOptions options;
  std::vector<TextDatabase::Match> results;
  Time first_time_searched;

  manager.GetTextMatches(L"FOO", options, &results, &first_time_searched);
  ASSERT_EQ(1, results.size());
  EXPECT_EQ(kTitle1, results[0].title);
}

// Like InsertCompleteNoVisit but specifies a visit to update. We check that the
// visit was updated properly.
TEST_F(TextDatabaseManagerTest, InsertCompleteVisit) {
  ASSERT_TRUE(Init());
  InMemVisitDB visit_db;
  TextDatabaseManager manager(dir_, &visit_db);
  ASSERT_TRUE(manager.Init());

  // First add a visit to a page. We can just make up a URL ID since there is
  // not actually any URL database around.
  VisitRow visit;
  visit.url_id = 1;
  visit.visit_time = Time::Now();
  visit.referring_visit = 0;
  visit.transition = PageTransition::LINK;
  visit.segment_id = 0;
  visit.is_indexed = false;
  visit_db.AddVisit(&visit);

  // Add a full text indexed entry for that visit.
  const GURL url(kURL2);
  manager.AddPageURL(url, visit.url_id, visit.visit_id, visit.visit_time);
  manager.AddPageContents(url, kBody2);
  manager.AddPageTitle(url, kTitle2);

  // Check that the page got added.
  QueryOptions options;
  std::vector<TextDatabase::Match> results;
  Time first_time_searched;

  manager.GetTextMatches(L"FOO", options, &results, &first_time_searched);
  ASSERT_EQ(1, results.size());
  EXPECT_EQ(kTitle2, results[0].title);

  // Check that the visit got updated for its new indexed state.
  VisitRow out_visit;
  ASSERT_TRUE(visit_db.GetRowForVisit(visit.visit_id, &out_visit));
  EXPECT_TRUE(out_visit.is_indexed);
}

// Tests that partial inserts that expire are added to the database.
TEST_F(TextDatabaseManagerTest, InsertPartial) {
  ASSERT_TRUE(Init());
  InMemVisitDB visit_db;
  TextDatabaseManager manager(dir_, &visit_db);
  ASSERT_TRUE(manager.Init());

  // Add the first one with just a URL.
  GURL url1(kURL1);
  manager.AddPageURL(url1, 0, 0, Time::Now());

  // Now add a second one with a URL and title.
  GURL url2(kURL2);
  manager.AddPageURL(url2, 0, 0, Time::Now());
  manager.AddPageTitle(url2, kTitle2);

  // The third one has a URL and body.
  GURL url3(kURL3);
  manager.AddPageURL(url3, 0, 0, Time::Now());
  manager.AddPageContents(url3, kBody3);

  // Expire stuff very fast. This assumes that the time between the first
  // AddPageURL and this line is less than the expiration time (20 seconds).
  TimeTicks added_time = TimeTicks::Now();
  TimeTicks expire_time = added_time + TimeDelta::FromSeconds(5);
  manager.FlushOldChangesForTime(expire_time);

  // Do a query, nothing should be added yet.
  QueryOptions options;
  std::vector<TextDatabase::Match> results;
  Time first_time_searched;
  manager.GetTextMatches(L"google", options, &results, &first_time_searched);
  ASSERT_EQ(0, results.size());

  // Compute a time threshold that will cause everything to be flushed, and
  // poke at the manager's internals to cause this to happen.
  expire_time = added_time + TimeDelta::FromDays(1);
  manager.FlushOldChangesForTime(expire_time);

  // Now we should have all 3 URLs added.
  manager.GetTextMatches(L"google", options, &results, &first_time_searched);
  ASSERT_EQ(3, results.size());
  EXPECT_TRUE(ResultsHaveURL(results, kURL1));
  EXPECT_TRUE(ResultsHaveURL(results, kURL2));
  EXPECT_TRUE(ResultsHaveURL(results, kURL3));
}

// Tests that changes get properly committed to disk.
TEST_F(TextDatabaseManagerTest, Writing) {
  ASSERT_TRUE(Init());

  QueryOptions options;
  std::vector<TextDatabase::Match> results;
  Time first_time_searched;

  InMemVisitDB visit_db;

  // Create the manager and write some stuff to it.
  {
    TextDatabaseManager manager(dir_, &visit_db);
    ASSERT_TRUE(manager.Init());

    std::vector<Time> times;
    AddAllPages(manager, &visit_db, &times);

    // We should have matched every page.
    manager.GetTextMatches(L"FOO", options, &results, &first_time_searched);
    EXPECT_EQ(6, results.size());
  }
  results.clear();

  // Recreate the manager and make sure it finds the written stuff.
  {
    TextDatabaseManager manager(dir_, &visit_db);
    ASSERT_TRUE(manager.Init());

    // We should have matched every page again.
    manager.GetTextMatches(L"FOO", options, &results, &first_time_searched);
    EXPECT_EQ(6, results.size());
  }
}

// Tests that changes get properly committed to disk, as in the Writing test
// above, but when there is a transaction around the adds.
TEST_F(TextDatabaseManagerTest, WritingTransaction) {
  ASSERT_TRUE(Init());

  QueryOptions options;
  std::vector<TextDatabase::Match> results;
  Time first_time_searched;

  InMemVisitDB visit_db;

  // Create the manager and write some stuff to it.
  {
    TextDatabaseManager manager(dir_, &visit_db);
    ASSERT_TRUE(manager.Init());

    std::vector<Time> times;
    manager.BeginTransaction();
    AddAllPages(manager, &visit_db, &times);
    // "Forget" to commit, it should be autocommittedd for us.

    // We should have matched every page.
    manager.GetTextMatches(L"FOO", options, &results, &first_time_searched);
    EXPECT_EQ(6, results.size());
  }
  results.clear();

  // Recreate the manager and make sure it finds the written stuff.
  {
    TextDatabaseManager manager(dir_, &visit_db);
    ASSERT_TRUE(manager.Init());

    // We should have matched every page again.
    manager.GetTextMatches(L"FOO", options, &results, &first_time_searched);
    EXPECT_EQ(6, results.size());
  }
}

// Tests querying where the maximum number of items is met.
TEST_F(TextDatabaseManagerTest, QueryMax) {
  ASSERT_TRUE(Init());
  InMemVisitDB visit_db;
  TextDatabaseManager manager(dir_, &visit_db);
  ASSERT_TRUE(manager.Init());

  std::vector<Time> times;
  AddAllPages(manager, &visit_db, &times);

  QueryOptions options;
  options.begin_time = times[0] - TimeDelta::FromDays(100);
  options.end_time = times[times.size() - 1] + TimeDelta::FromDays(100);
  options.max_count = 2;
  std::vector<TextDatabase::Match> results;
  Time first_time_searched;
  manager.GetTextMatches(L"FOO", options, &results, &first_time_searched);

  // We should have gotten the last two pages as results (the first page is
  // also the last).
  EXPECT_EQ(2, results.size());
  EXPECT_TRUE(first_time_searched <= times[4]);
  EXPECT_TRUE(ResultsHaveURL(results, kURL5));
  EXPECT_TRUE(ResultsHaveURL(results, kURL1));

  // Asking for 4 pages, the first one should be in another DB.
  options.max_count = 4;
  manager.GetTextMatches(L"FOO", options, &results, &first_time_searched);

  EXPECT_EQ(4, results.size());
  EXPECT_TRUE(first_time_searched <= times[4]);
  EXPECT_TRUE(ResultsHaveURL(results, kURL3));
  EXPECT_TRUE(ResultsHaveURL(results, kURL4));
  EXPECT_TRUE(ResultsHaveURL(results, kURL5));
  EXPECT_TRUE(ResultsHaveURL(results, kURL1));
}

// Tests querying backwards in time in chunks.
TEST_F(TextDatabaseManagerTest, QueryBackwards) {
  ASSERT_TRUE(Init());
  InMemVisitDB visit_db;
  TextDatabaseManager manager(dir_, &visit_db);
  ASSERT_TRUE(manager.Init());

  std::vector<Time> times;
  AddAllPages(manager, &visit_db, &times);

  // First do a query for all time, but with a max of 2. This will give us the
  // last two results and will tell us where to start searching when we want
  // to go back in time.
  QueryOptions options;
  options.begin_time = times[0] - TimeDelta::FromDays(100);
  options.end_time = times[times.size() - 1] + TimeDelta::FromDays(100);
  options.max_count = 2;
  std::vector<TextDatabase::Match> results;
  Time first_time_searched;
  manager.GetTextMatches(L"FOO", options, &results, &first_time_searched);

  // Check that we got the last two results.
  EXPECT_EQ(2, results.size());
  EXPECT_TRUE(first_time_searched <= times[4]);
  EXPECT_TRUE(ResultsHaveURL(results, kURL5));
  EXPECT_TRUE(ResultsHaveURL(results, kURL1));

  // Query the previous two URLs and make sure we got the correct ones.
  options.end_time = first_time_searched;
  manager.GetTextMatches(L"FOO", options, &results, &first_time_searched);
  EXPECT_EQ(2, results.size());
  EXPECT_TRUE(first_time_searched <= times[2]);
  EXPECT_TRUE(ResultsHaveURL(results, kURL3));
  EXPECT_TRUE(ResultsHaveURL(results, kURL4));

  // Query the previous two URLs...
  options.end_time = first_time_searched;
  manager.GetTextMatches(L"FOO", options, &results, &first_time_searched);
  EXPECT_EQ(2, results.size());
  EXPECT_TRUE(first_time_searched <= times[0]);
  EXPECT_TRUE(ResultsHaveURL(results, kURL2));
  EXPECT_TRUE(ResultsHaveURL(results, kURL1));

  // Try to query some more, there should be no results.
  options.end_time = first_time_searched;
  manager.GetTextMatches(L"FOO", options, &results, &first_time_searched);
  EXPECT_EQ(0, results.size());
}

}  // namespace history