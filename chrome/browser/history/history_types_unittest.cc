// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/history_types.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;

namespace history {

namespace {

// Validates the consistency of the given history result. We just make sure
// that the URL rows match the indices structure. The unit tests themselves
// test the index structure to verify things are in the right order, so we
// don't need to.
void CheckHistoryResultConsistency(const QueryResults& result) {
  for (size_t i = 0; i < result.size(); i++) {
    size_t match_count;
    const size_t* matches = result.MatchesForURL(result[i].url(), &match_count);

    bool found = false;
    for (size_t match = 0; match < match_count; match++) {
      if (matches[match] == i) {
        found = true;
        break;
      }
    }

    EXPECT_TRUE(found) << "The URL had no index referring to it.";
  }
}

static const char kURL1[] = "http://www.google.com/";
static const char kURL2[] = "http://news.google.com/";
static const char kURL3[] = "http://images.google.com/";

// Adds kURL1 twice and kURL2 once.
void AddSimpleData(QueryResults* results) {
  GURL url1(kURL1);
  GURL url2(kURL2);
  URLResult result1(url1, Time::Now());
  URLResult result2(url1, Time::Now());
  URLResult result3(url2, Time::Now());

  // The URLResults are invalid after being inserted.
  results->AppendURLBySwapping(&result1);
  results->AppendURLBySwapping(&result2);
  results->AppendURLBySwapping(&result3);
  CheckHistoryResultConsistency(*results);
}

// Adds kURL2 once and kURL3 once.
void AddAlternateData(QueryResults* results) {
  GURL url2(kURL2);
  GURL url3(kURL3);
  URLResult result1(url2, Time::Now());
  URLResult result2(url3, Time::Now());

  // The URLResults are invalid after being inserted.
  results->AppendURLBySwapping(&result1);
  results->AppendURLBySwapping(&result2);
  CheckHistoryResultConsistency(*results);
}

}  // namespace

// Tests insertion and deletion by range.
TEST(HistoryQueryResult, DeleteRange) {
  GURL url1(kURL1);
  GURL url2(kURL2);
  QueryResults results;
  AddSimpleData(&results);

  // Make sure the first URL is in there twice. The indices can be in either
  // order.
  size_t match_count;
  const size_t* matches = results.MatchesForURL(url1, &match_count);
  ASSERT_EQ(2U, match_count);
  EXPECT_TRUE((matches[0] == 0 && matches[1] == 1) ||
              (matches[0] == 1 && matches[1] == 0));

  // Check the second one.
  matches = results.MatchesForURL(url2, &match_count);
  ASSERT_EQ(1U, match_count);
  EXPECT_TRUE(matches[0] == 2);

  // Delete the first instance of the first URL.
  results.DeleteRange(0, 0);
  CheckHistoryResultConsistency(results);

  // Check the two URLs.
  matches = results.MatchesForURL(url1, &match_count);
  ASSERT_EQ(1U, match_count);
  EXPECT_TRUE(matches[0] == 0);
  matches = results.MatchesForURL(url2, &match_count);
  ASSERT_EQ(1U, match_count);
  EXPECT_TRUE(matches[0] == 1);

  // Now delete everything and make sure it's deleted.
  results.DeleteRange(0, 1);
  EXPECT_EQ(0U, results.size());
  EXPECT_FALSE(results.MatchesForURL(url1, NULL));
  EXPECT_FALSE(results.MatchesForURL(url2, NULL));
}

// Tests insertion and deletion by URL.
TEST(HistoryQueryResult, ResultDeleteURL) {
  GURL url1(kURL1);
  GURL url2(kURL2);
  QueryResults results;
  AddSimpleData(&results);

  // Delete the first URL.
  results.DeleteURL(url1);
  CheckHistoryResultConsistency(results);
  EXPECT_EQ(1U, results.size());

  // The first one should be gone, and the second one should be at [0].
  size_t match_count;
  EXPECT_FALSE(results.MatchesForURL(url1, NULL));
  const size_t* matches = results.MatchesForURL(url2, &match_count);
  ASSERT_EQ(1U, match_count);
  EXPECT_TRUE(matches[0] == 0);

  // Delete the second URL, there should be nothing left.
  results.DeleteURL(url2);
  EXPECT_EQ(0U, results.size());
  EXPECT_FALSE(results.MatchesForURL(url2, NULL));
}

TEST(HistoryQueryResult, AppendResults) {
  GURL url1(kURL1);
  GURL url2(kURL2);
  GURL url3(kURL3);

  // This is the base.
  QueryResults results;
  AddSimpleData(&results);

  // Now create the appendee.
  QueryResults appendee;
  AddAlternateData(&appendee);

  results.AppendResultsBySwapping(&appendee, true);
  CheckHistoryResultConsistency(results);

  // There should be 3 results, the second one of the appendee should be
  // deleted because it was already in the first one and we said remove dupes.
  ASSERT_EQ(4U, results.size());

  // The first URL should be unchanged in the first two spots.
  size_t match_count;
  const size_t* matches = results.MatchesForURL(url1, &match_count);
  ASSERT_EQ(2U, match_count);
  EXPECT_TRUE((matches[0] == 0 && matches[1] == 1) ||
              (matches[0] == 1 && matches[1] == 0));

  // The second URL should be there once after that
  matches = results.MatchesForURL(url2, &match_count);
  ASSERT_EQ(1U, match_count);
  EXPECT_TRUE(matches[0] == 2);

  // The third one should be after that.
  matches = results.MatchesForURL(url3, &match_count);
  ASSERT_EQ(1U, match_count);
  EXPECT_TRUE(matches[0] == 3);
}

}  // namespace
