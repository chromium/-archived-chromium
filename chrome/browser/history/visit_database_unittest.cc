// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "chrome/browser/history/url_database.h"
#include "chrome/browser/history/visit_database.h"
#include "chrome/common/sqlite_compiled_statement.h"
#include "chrome/common/sqlite_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using base::Time;
using base::TimeDelta;

namespace history {

namespace {

bool IsVisitInfoEqual(const VisitRow& a,
                      const VisitRow& b) {
  return a.visit_id == b.visit_id &&
         a.url_id == b.url_id &&
         a.visit_time == b.visit_time &&
         a.referring_visit == b.referring_visit &&
         a.transition == b.transition &&
         a.is_indexed == b.is_indexed;
}

}  // namespace

class VisitDatabaseTest : public PlatformTest,
                          public URLDatabase,
                          public VisitDatabase {
 public:
  VisitDatabaseTest() : db_(NULL), statement_cache_(NULL) {
  }

 private:
  // Test setup.
  void SetUp() {
    PlatformTest::SetUp();
    PathService::Get(base::DIR_TEMP, &db_file_);
    db_file_.push_back(FilePath::kSeparators[0]);
    db_file_.append(L"VisitTest.db");
    file_util::Delete(db_file_, false);

    EXPECT_EQ(SQLITE_OK, sqlite3_open(WideToUTF8(db_file_).c_str(), &db_));
    statement_cache_ = new SqliteStatementCache(db_);

    // Initialize the tables for this test.
    CreateURLTable(false);
    CreateMainURLIndex();
    CreateSupplimentaryURLIndices();
    InitVisitTable();
  }
  void TearDown() {
    delete statement_cache_;
    sqlite3_close(db_);
    file_util::Delete(db_file_, false);
    PlatformTest::TearDown();
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

TEST_F(VisitDatabaseTest, Add) {
  // Add one visit.
  VisitRow visit_info1(1, Time::Now(), 0, PageTransition::LINK, 0);
  EXPECT_TRUE(AddVisit(&visit_info1));

  // Add second visit for the same page.
  VisitRow visit_info2(visit_info1.url_id,
      visit_info1.visit_time + TimeDelta::FromSeconds(1), 1,
      PageTransition::TYPED, 0);
  EXPECT_TRUE(AddVisit(&visit_info2));

  // Add third visit for a different page.
  VisitRow visit_info3(2,
      visit_info1.visit_time + TimeDelta::FromSeconds(2), 0,
      PageTransition::LINK, 0);
  EXPECT_TRUE(AddVisit(&visit_info3));

  // Query the first two.
  std::vector<VisitRow> matches;
  EXPECT_TRUE(GetVisitsForURL(visit_info1.url_id, &matches));
  EXPECT_EQ(static_cast<size_t>(2), matches.size());

  // Make sure we got both (order in result set is visit time).
  EXPECT_TRUE(IsVisitInfoEqual(matches[0], visit_info1) &&
              IsVisitInfoEqual(matches[1], visit_info2));
}

TEST_F(VisitDatabaseTest, Delete) {
  // Add three visits that form a chain of navigation, and then delete the
  // middle one. We should be left with the outer two visits, and the chain
  // should link them.
  static const int kTime1 = 1000;
  VisitRow visit_info1(1, Time::FromInternalValue(kTime1), 0,
                       PageTransition::LINK, 0);
  EXPECT_TRUE(AddVisit(&visit_info1));

  static const int kTime2 = kTime1 + 1;
  VisitRow visit_info2(1, Time::FromInternalValue(kTime2),
                       visit_info1.visit_id, PageTransition::LINK, 0);
  EXPECT_TRUE(AddVisit(&visit_info2));

  static const int kTime3 = kTime2 + 1;
  VisitRow visit_info3(1, Time::FromInternalValue(kTime3),
                       visit_info2.visit_id, PageTransition::LINK, 0);
  EXPECT_TRUE(AddVisit(&visit_info3));

  // First make sure all the visits are there.
  std::vector<VisitRow> matches;
  EXPECT_TRUE(GetVisitsForURL(visit_info1.url_id, &matches));
  EXPECT_EQ(static_cast<size_t>(3), matches.size());
  EXPECT_TRUE(IsVisitInfoEqual(matches[0], visit_info1) &&
              IsVisitInfoEqual(matches[1], visit_info2) &&
              IsVisitInfoEqual(matches[2], visit_info3));

  // Delete the middle one.
  DeleteVisit(visit_info2);

  // The outer two should be left, and the last one should have the first as
  // the referrer.
  visit_info3.referring_visit = visit_info1.visit_id;
  matches.clear();
  EXPECT_TRUE(GetVisitsForURL(visit_info1.url_id, &matches));
  EXPECT_EQ(static_cast<size_t>(2), matches.size());
  EXPECT_TRUE(IsVisitInfoEqual(matches[0], visit_info1) &&
              IsVisitInfoEqual(matches[1], visit_info3));
}

TEST_F(VisitDatabaseTest, Update) {
  // Make something in the database.
  VisitRow original(1, Time::Now(), 23, 22, 19);
  AddVisit(&original);

  // Mutate that row.
  VisitRow modification(original);
  modification.url_id = 2;
  modification.transition = PageTransition::TYPED;
  modification.visit_time = Time::Now() + TimeDelta::FromDays(1);
  modification.referring_visit = 9292;
  modification.is_indexed = true;
  UpdateVisitRow(modification);

  // Check that the mutated version was written.
  VisitRow final;
  GetRowForVisit(original.visit_id, &final);
  EXPECT_TRUE(IsVisitInfoEqual(modification, final));
}

// TODO(brettw) write test for GetMostRecentVisitForURL!

#if defined(OS_WIN)
// TODO(playmobil): Enable on POSIX
TEST_F(VisitDatabaseTest, GetVisibleVisitsInRange) {
  // Add one visit.
  VisitRow visit_info1(1, Time::Now(), 0,
      static_cast<PageTransition::Type>(PageTransition::LINK |
                                        PageTransition::CHAIN_START |
                                        PageTransition::CHAIN_END),
      0);
  visit_info1.visit_id = 1;
  EXPECT_TRUE(AddVisit(&visit_info1));

  // Add second visit for the same page.
  VisitRow visit_info2(visit_info1.url_id,
      visit_info1.visit_time + TimeDelta::FromSeconds(1), 1,
      static_cast<PageTransition::Type>(PageTransition::TYPED |
                                        PageTransition::CHAIN_START |
                                        PageTransition::CHAIN_END),
      0);
  visit_info2.visit_id = 2;
  EXPECT_TRUE(AddVisit(&visit_info2));

  // Add third visit for a different page.
  VisitRow visit_info3(2,
      visit_info1.visit_time + TimeDelta::FromSeconds(2), 0,
      static_cast<PageTransition::Type>(PageTransition::LINK |
                                        PageTransition::CHAIN_START),
      0);
  visit_info3.visit_id = 3;
  EXPECT_TRUE(AddVisit(&visit_info3));

  // Add a redirect visit from the last page.
  VisitRow visit_info4(3,
      visit_info1.visit_time + TimeDelta::FromSeconds(3), visit_info3.visit_id,
      static_cast<PageTransition::Type>(PageTransition::SERVER_REDIRECT |
                                        PageTransition::CHAIN_END),
      0);
  visit_info4.visit_id = 4;
  EXPECT_TRUE(AddVisit(&visit_info4));

  // Add a subframe visit.
  VisitRow visit_info5(4,
      visit_info1.visit_time + TimeDelta::FromSeconds(4), visit_info4.visit_id,
      static_cast<PageTransition::Type>(PageTransition::AUTO_SUBFRAME |
                                        PageTransition::CHAIN_START |
                                        PageTransition::CHAIN_END),
      0);
  visit_info5.visit_id = 5;
  EXPECT_TRUE(AddVisit(&visit_info5));

  // Query the visits for all time, we should get the first 3 in descending
  // order, but not the redirect & subframe ones later.
  VisitVector results;
  GetVisibleVisitsInRange(Time(), Time(), false, 0, &results);
  ASSERT_EQ(static_cast<size_t>(3), results.size());
  EXPECT_TRUE(IsVisitInfoEqual(results[0], visit_info4) &&
              IsVisitInfoEqual(results[1], visit_info2) &&
              IsVisitInfoEqual(results[2], visit_info1));

  // If we want only the most recent one, it should give us the same results
  // minus the first (duplicate of the second) one.
  GetVisibleVisitsInRange(Time(), Time(), true, 0, &results);
  ASSERT_EQ(static_cast<size_t>(2), results.size());
  EXPECT_TRUE(IsVisitInfoEqual(results[0], visit_info4) &&
              IsVisitInfoEqual(results[1], visit_info2));

  // Query a time range and make sure beginning is inclusive and ending is
  // exclusive.
  GetVisibleVisitsInRange(visit_info2.visit_time, visit_info4.visit_time,
                          false, 0, &results);
  ASSERT_EQ(static_cast<size_t>(1), results.size());
  EXPECT_TRUE(IsVisitInfoEqual(results[0], visit_info2));

  // Query for a max count and make sure we get only that number.
  GetVisibleVisitsInRange(Time(), Time(), false, 2, &results);
  ASSERT_EQ(static_cast<size_t>(2), results.size());
  EXPECT_TRUE(IsVisitInfoEqual(results[0], visit_info4) &&
              IsVisitInfoEqual(results[1], visit_info2));
}
#endif  // defined(OS_WIN)
}  // namespace history
