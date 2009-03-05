// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "chrome/browser/history/url_database.h"
#include "chrome/common/sqlite_compiled_statement.h"
#include "chrome/common/sqlite_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;
using base::TimeDelta;

namespace history {

namespace {

bool IsURLRowEqual(const URLRow& a,
                   const URLRow& b) {
  // TODO(brettw) when the database stores an actual Time value rather than
  // a time_t, do a reaul comparison. Instead, we have to do a more rough
  // comparison since the conversion reduces the precision.
  return a.title() == b.title() &&
      a.visit_count() == b.visit_count() &&
      a.typed_count() == b.typed_count() &&
      a.last_visit() - b.last_visit() <= TimeDelta::FromSeconds(1) &&
      a.hidden() == b.hidden();
}

}  // namespace

class URLDatabaseTest : public testing::Test,
                        public URLDatabase {
 public:
  URLDatabaseTest() : db_(NULL), statement_cache_(NULL) {
  }

 private:
  // Test setup.
  void SetUp() {
    PathService::Get(base::DIR_TEMP, &db_file_);
    db_file_.push_back(FilePath::kSeparators[0]);
    db_file_.append(L"URLTest.db");

    EXPECT_EQ(SQLITE_OK, sqlite3_open(WideToUTF8(db_file_).c_str(), &db_));
    statement_cache_ = new SqliteStatementCache(db_);

    // Initialize the tables for this test.
    CreateURLTable(false);
    CreateMainURLIndex();
    CreateSupplimentaryURLIndices();
    InitKeywordSearchTermsTable();
  }
  void TearDown() {
    delete statement_cache_;
    sqlite3_close(db_);
    file_util::Delete(db_file_, false);
  }

  // Provided for URL/VisitDatabase.
  virtual sqlite3* GetDB() {
    return db_;
  }
  virtual SqliteStatementCache& GetStatementCache() {
    return *statement_cache_;
  }

  std::wstring db_file_;
  sqlite3* db_;
  SqliteStatementCache* statement_cache_;
};

// Test add and query for the URL table in the HistoryDatabase
TEST_F(URLDatabaseTest, AddURL) {
  // first, add two URLs
  const GURL url1("http://www.google.com/");
  URLRow url_info1(url1);
  url_info1.set_title(L"Google");
  url_info1.set_visit_count(4);
  url_info1.set_typed_count(2);
  url_info1.set_last_visit(Time::Now() - TimeDelta::FromDays(1));
  url_info1.set_hidden(false);
  EXPECT_TRUE(AddURL(url_info1));

  const GURL url2("http://mail.google.com/");
  URLRow url_info2(url2);
  url_info2.set_title(L"Google Mail");
  url_info2.set_visit_count(3);
  url_info2.set_typed_count(0);
  url_info2.set_last_visit(Time::Now() - TimeDelta::FromDays(2));
  url_info2.set_hidden(true);
  EXPECT_TRUE(AddURL(url_info2));

  // query both of them
  URLRow info;
  EXPECT_TRUE(GetRowForURL(url1, &info));
  EXPECT_TRUE(IsURLRowEqual(url_info1, info));
  URLID id2 = GetRowForURL(url2, &info);
  EXPECT_TRUE(id2);
  EXPECT_TRUE(IsURLRowEqual(url_info2, info));

  // update the second
  url_info2.set_title(L"Google Mail Too");
  url_info2.set_visit_count(4);
  url_info2.set_typed_count(1);
  url_info2.set_typed_count(91011);
  url_info2.set_hidden(false);
  EXPECT_TRUE(UpdateURLRow(id2, url_info2));

  // make sure it got updated
  URLRow info2;
  EXPECT_TRUE(GetRowForURL(url2, &info2));
  EXPECT_TRUE(IsURLRowEqual(url_info2, info2));

  // query a nonexistant URL
  EXPECT_EQ(0, GetRowForURL(GURL("http://news.google.com/"), &info));

  // Delete all urls in the domain
  // FIXME(ACW) test the new url based delete domain
  //EXPECT_TRUE(db.DeleteDomain(kDomainID));

  // Make sure the urls have been properly removed
  // FIXME(ACW) commented out because remove no longer works.
  //EXPECT_TRUE(db.GetURLInfo(url1, NULL) == NULL);
  //EXPECT_TRUE(db.GetURLInfo(url2, NULL) == NULL);
}

// Tests adding, querying and deleting keyword visits.
TEST_F(URLDatabaseTest, KeywordSearchTermVisit) {
  const GURL url1("http://www.google.com/");
  URLRow url_info1(url1);
  url_info1.set_title(L"Google");
  url_info1.set_visit_count(4);
  url_info1.set_typed_count(2);
  url_info1.set_last_visit(Time::Now() - TimeDelta::FromDays(1));
  url_info1.set_hidden(false);
  URLID url_id = AddURL(url_info1);
  ASSERT_TRUE(url_id != 0);

  // Add a keyword visit.
  ASSERT_TRUE(SetKeywordSearchTermsForURL(url_id, 1, L"visit"));

  // Make sure we get it back.
  std::vector<KeywordSearchTermVisit> matches;
  GetMostRecentKeywordSearchTerms(1, L"visit", 10, &matches);
  ASSERT_EQ(1U, matches.size());
  ASSERT_EQ(L"visit", matches[0].term);

  // Delete the keyword visit.
  DeleteAllSearchTermsForKeyword(1);

  // Make sure we don't get it back when querying.
  matches.clear();
  GetMostRecentKeywordSearchTerms(1, L"visit", 10, &matches);
  ASSERT_EQ(0U, matches.size());
}

// Make sure deleting a URL also deletes a keyword visit.
TEST_F(URLDatabaseTest, DeleteURLDeletesKeywordSearchTermVisit) {
  const GURL url1("http://www.google.com/");
  URLRow url_info1(url1);
  url_info1.set_title(L"Google");
  url_info1.set_visit_count(4);
  url_info1.set_typed_count(2);
  url_info1.set_last_visit(Time::Now() - TimeDelta::FromDays(1));
  url_info1.set_hidden(false);
  URLID url_id = AddURL(url_info1);
  ASSERT_TRUE(url_id != 0);

  // Add a keyword visit.
  ASSERT_TRUE(SetKeywordSearchTermsForURL(url_id, 1, L"visit"));

  // Delete the url.
  ASSERT_TRUE(DeleteURLRow(url_id));

  // Make sure the keyword visit was deleted.
  std::vector<KeywordSearchTermVisit> matches;
  GetMostRecentKeywordSearchTerms(1, L"visit", 10, &matches);
  ASSERT_EQ(0U, matches.size());
}

}  // namespace history
