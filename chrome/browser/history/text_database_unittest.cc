// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "chrome/browser/history/text_database.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using base::Time;

namespace history {

namespace {

// Note that all pages have "COUNTTAG" which allows us to count the number of
// pages in the database withoujt adding any extra functions to the DB object.
const char kURL1[] = "http://www.google.com/";
const int kTime1 = 1000;
const char kTitle1[] = "Google";
const char kBody1[] =
    "COUNTTAG Web Images Maps News Shopping Gmail more My Account | "
    "Sign out Advanced Search Preferences Language Tools Advertising Programs "
    "- Business Solutions - About Google, 2008 Google";

const char kURL2[] = "http://images.google.com/";
const int kTime2 = 2000;
const char kTitle2[] = "Google Image Search";
const char kBody2[] =
    "COUNTTAG Web Images Maps News Shopping Gmail more My Account | "
    "Sign out Advanced Image Search Preferences The most comprehensive image "
    "search on the web. Want to help improve Google Image Search? Try Google "
    "Image Labeler. Advertising Programs - Business Solutions - About Google "
    "2008 Google";

const char kURL3[] = "http://slashdot.org/";
const int kTime3 = 3000;
const char kTitle3[] = "Slashdot: News for nerds, stuff that matters";
const char kBody3[] =
    "COUNTTAG Slashdot Log In Create Account Subscribe Firehose Why "
    "Log In? Why Subscribe? Nickname Password Public Terminal Sections "
    "Main Apple AskSlashdot Backslash Books Developers Games Hardware "
    "Interviews IT Linux Mobile Politics Science YRO";

// Returns the number of rows currently in the database.
int RowCount(TextDatabase* db) {
  QueryOptions options;
  options.begin_time = Time::FromInternalValue(0);
  // Leave end_time at now.

  std::vector<TextDatabase::Match> results;
  Time first_time_searched;
  TextDatabase::URLSet unique_urls;
  db->GetTextMatches("COUNTTAG", options, &results, &unique_urls,
                     &first_time_searched);
  return static_cast<int>(results.size());
}

// Adds each of the test pages to the database.
void AddAllTestData(TextDatabase* db) {
  EXPECT_TRUE(db->AddPageData(
      Time::FromInternalValue(kTime1), kURL1, kTitle1, kBody1));
  EXPECT_TRUE(db->AddPageData(
      Time::FromInternalValue(kTime2), kURL2, kTitle2, kBody2));
  EXPECT_TRUE(db->AddPageData(
      Time::FromInternalValue(kTime3), kURL3, kTitle3, kBody3));
  EXPECT_EQ(3, RowCount(db));
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

class TextDatabaseTest : public PlatformTest {
 public:
  TextDatabaseTest() : db_(NULL) {
  }

 protected:
  void SetUp() {
    PlatformTest::SetUp();
    PathService::Get(base::DIR_TEMP, &temp_path_);
  }

  void TearDown() {
    for (size_t i = 0; i < opened_files_.size(); i++)
      file_util::Delete(opened_files_[i], false);
    file_util::Delete(file_name_, false);
    PlatformTest::TearDown();
  }

  // Create databases with this function, which will ensure that the files are
  // deleted on shutdown. Only open one database for each file. Returns NULL on
  // failure.
  //
  // Set |delete_file| to delete any existing file. If we are trying to create
  // the file for the first time, we don't want a previous test left in a
  // weird state to have left a file that would affect us.
  TextDatabase* CreateDB(TextDatabase::DBIdent id,
                         bool allow_create,
                         bool delete_file) {
    TextDatabase* db = new TextDatabase(temp_path_, id, allow_create);

    if (delete_file)
      file_util::Delete(db->file_name(), false);

    if (!db->Init()) {
      delete db;
      return NULL;
    }
    opened_files_.push_back(db->file_name());
    return db;
  }

  // Directory containing the databases.
  std::wstring temp_path_;

  // Name of the main database file.
  std::wstring file_name_;
  sqlite3* db_;

  std::vector<std::wstring> opened_files_;
};

TEST_F(TextDatabaseTest, AttachDetach) {
  // First database with one page.
  const int kIdee1 = 200801;
  scoped_ptr<TextDatabase> db1(CreateDB(kIdee1, true, true));
  ASSERT_TRUE(!!db1.get());
  EXPECT_TRUE(db1->AddPageData(
      Time::FromInternalValue(kTime1), kURL1, kTitle1, kBody1));

  // Second database with one page.
  const int kIdee2 = 200802;
  scoped_ptr<TextDatabase> db2(CreateDB(kIdee2, true, true));
  ASSERT_TRUE(!!db2.get());
  EXPECT_TRUE(db2->AddPageData(
      Time::FromInternalValue(kTime2), kURL2, kTitle2, kBody2));

  // Detach, then reattach database one. The file should exist, so we force
  // opening an existing file.
  db1.reset();
  db1.reset(CreateDB(kIdee1, false, false));
  ASSERT_TRUE(!!db1.get());

  // We should not be able to attach this random database for which no file
  // exists.
  const int kIdeeNoExisto = 999999999;
  scoped_ptr<TextDatabase> db3(CreateDB(kIdeeNoExisto, false, true));
  EXPECT_FALSE(!!db3.get());
}

TEST_F(TextDatabaseTest, AddRemove) {
  // Create a database and add some pages to it.
  const int kIdee1 = 200801;
  scoped_ptr<TextDatabase> db(CreateDB(kIdee1, true, true));
  ASSERT_TRUE(!!db.get());
  URLID id1 = db->AddPageData(
      Time::FromInternalValue(kTime1), kURL1, kTitle1, kBody1);
  EXPECT_NE(0, id1);
  URLID id2 = db->AddPageData(
      Time::FromInternalValue(kTime2), kURL2, kTitle2, kBody2);
  EXPECT_NE(0, id2);
  URLID id3 = db->AddPageData(
      Time::FromInternalValue(kTime3), kURL3, kTitle3, kBody3);
  EXPECT_NE(0, id3);
  EXPECT_EQ(3, RowCount(db.get()));

  // Make sure we can delete some of the data.
  db->DeletePageData(Time::FromInternalValue(kTime1), kURL1);
  EXPECT_EQ(2, RowCount(db.get()));

  // Close and reopen.
  db.reset(new TextDatabase(temp_path_, kIdee1, false));
  EXPECT_TRUE(db->Init());

  // Verify that the deleted ID is gone and try to delete another one.
  EXPECT_EQ(2, RowCount(db.get()));
  db->DeletePageData(Time::FromInternalValue(kTime2), kURL2);
  EXPECT_EQ(1, RowCount(db.get()));
}

TEST_F(TextDatabaseTest, Query) {
  // Make a database with some pages.
  const int kIdee1 = 200801;
  scoped_ptr<TextDatabase> db(CreateDB(kIdee1, true, true));
  EXPECT_TRUE(!!db.get());
  AddAllTestData(db.get());

  // Get all the results.
  QueryOptions options;
  options.begin_time = Time::FromInternalValue(0);

  std::vector<TextDatabase::Match> results;
  Time first_time_searched;
  TextDatabase::URLSet unique_urls;
  db->GetTextMatches("COUNTTAG", options, &results, &unique_urls,
                     &first_time_searched);
  EXPECT_TRUE(unique_urls.empty()) << "Didn't ask for unique URLs";

  // All 3 sites should be returned in order.
  ASSERT_EQ(3U, results.size());
  EXPECT_EQ(GURL(kURL1), results[2].url);
  EXPECT_EQ(GURL(kURL2), results[1].url);
  EXPECT_EQ(GURL(kURL3), results[0].url);

  // Verify the info on those results.
  EXPECT_TRUE(Time::FromInternalValue(kTime1) == results[2].time);
  EXPECT_TRUE(Time::FromInternalValue(kTime2) == results[1].time);
  EXPECT_TRUE(Time::FromInternalValue(kTime3) == results[0].time);

  EXPECT_EQ(UTF8ToWide(std::string(kTitle1)), results[2].title);
  EXPECT_EQ(UTF8ToWide(std::string(kTitle2)), results[1].title);
  EXPECT_EQ(UTF8ToWide(std::string(kTitle3)), results[0].title);

  // Should have no matches in the title.
  EXPECT_EQ(0U, results[0].title_match_positions.size());
  EXPECT_EQ(0U, results[1].title_match_positions.size());
  EXPECT_EQ(0U, results[2].title_match_positions.size());

  // We don't want to be dependent on the exact snippet algorithm, but we know
  // since we searched for "COUNTTAG" which occurs at the beginning of each
  // document, that each snippet should start with that.
  EXPECT_TRUE(StartsWithASCII(WideToUTF8(results[0].snippet.text()),
                              "COUNTTAG", false));
  EXPECT_TRUE(StartsWithASCII(WideToUTF8(results[1].snippet.text()),
                              "COUNTTAG", false));
  EXPECT_TRUE(StartsWithASCII(WideToUTF8(results[2].snippet.text()),
                              "COUNTTAG", false));
}

TEST_F(TextDatabaseTest, TimeRange) {
  // Make a database with some pages.
  const int kIdee1 = 200801;
  scoped_ptr<TextDatabase> db(CreateDB(kIdee1, true, true));
  ASSERT_TRUE(!!db.get());
  AddAllTestData(db.get());

  // Beginning should be inclusive, and the ending exclusive.
  // Get all the results.
  QueryOptions options;
  options.begin_time = Time::FromInternalValue(kTime1);
  options.end_time = Time::FromInternalValue(kTime3);

  std::vector<TextDatabase::Match> results;
  Time first_time_searched;
  TextDatabase::URLSet unique_urls;
  db->GetTextMatches("COUNTTAG", options, &results,  &unique_urls,
                     &first_time_searched);
  EXPECT_TRUE(unique_urls.empty()) << "Didn't ask for unique URLs";

  // The first and second should have been returned.
  EXPECT_EQ(2U, results.size());
  EXPECT_TRUE(ResultsHaveURL(results, kURL1));
  EXPECT_TRUE(ResultsHaveURL(results, kURL2));
  EXPECT_FALSE(ResultsHaveURL(results, kURL3));
  EXPECT_EQ(kTime1, first_time_searched.ToInternalValue());

  // ---------------------------------------------------------------------------
  // Do a query where there isn't a result on the begin boundary, so we can
  // test that the first time searched is set to the minimum time considered
  // instead of the min value.
  options.begin_time = Time::FromInternalValue((kTime2 - kTime1) / 2 + kTime1);
  options.end_time = Time::FromInternalValue(kTime3 + 1);
  results.clear();  // GetTextMatches does *not* clear the results.
  db->GetTextMatches("COUNTTAG", options, &results, &unique_urls,
                     &first_time_searched);
  EXPECT_TRUE(unique_urls.empty()) << "Didn't ask for unique URLs";
  EXPECT_EQ(options.begin_time.ToInternalValue(),
            first_time_searched.ToInternalValue());

  // Should have two results, the second and third.
  EXPECT_EQ(2U, results.size());
  EXPECT_FALSE(ResultsHaveURL(results, kURL1));
  EXPECT_TRUE(ResultsHaveURL(results, kURL2));
  EXPECT_TRUE(ResultsHaveURL(results, kURL3));

  // No results should also set the first_time_searched.
  options.begin_time = Time::FromInternalValue(kTime3 + 1);
  options.end_time = Time::FromInternalValue(kTime3 * 100);
  results.clear();
  db->GetTextMatches("COUNTTAG", options, &results, &unique_urls,
                     &first_time_searched);
  EXPECT_EQ(options.begin_time.ToInternalValue(),
            first_time_searched.ToInternalValue());
}

// Make sure that max_count works.
TEST_F(TextDatabaseTest, MaxCount) {
  // Make a database with some pages.
  const int kIdee1 = 200801;
  scoped_ptr<TextDatabase> db(CreateDB(kIdee1, true, true));
  ASSERT_TRUE(!!db.get());
  AddAllTestData(db.get());

  // Set up the query to return all the results with "Google" (should be 2), but
  // with a maximum of 1.
  QueryOptions options;
  options.begin_time = Time::FromInternalValue(kTime1);
  options.end_time = Time::FromInternalValue(kTime3 + 1);
  options.max_count = 1;

  std::vector<TextDatabase::Match> results;
  Time first_time_searched;
  TextDatabase::URLSet unique_urls;
  db->GetTextMatches("google", options, &results, &unique_urls,
                     &first_time_searched);
  EXPECT_TRUE(unique_urls.empty()) << "Didn't ask for unique URLs";

  // There should be one result, the most recent one.
  EXPECT_EQ(1U, results.size());
  EXPECT_TRUE(ResultsHaveURL(results, kURL2));

  // The max time considered should be the date of the returned item.
  EXPECT_EQ(kTime2, first_time_searched.ToInternalValue());
}

}  // namespace history
