// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"

typedef testing::Test BookmarkUtilsTest;

TEST_F(BookmarkUtilsTest, GetBookmarksContainingText) {
  BookmarkModel model(NULL);
  BookmarkNode* n1 =
      model.AddURL(model.other_node(), 0, L"foo bar",
                   GURL("http://www.google.com"));
  BookmarkNode* n2 =
      model.AddURL(model.other_node(), 0, L"baz buz",
                   GURL("http://www.cnn.com"));

  model.AddGroup(model.other_node(), 0, L"foo");

  std::vector<BookmarkNode*> nodes;
  bookmark_utils::GetBookmarksContainingText(&model, L"foo", 100, &nodes);
  ASSERT_EQ(1U, nodes.size());
  EXPECT_TRUE(nodes[0] == n1);
  EXPECT_TRUE(bookmark_utils::DoesBookmarkContainText(n1, L"foo"));
  nodes.clear();

  bookmark_utils::GetBookmarksContainingText(&model, L"cnn", 100, &nodes);
  ASSERT_EQ(1U, nodes.size());
  EXPECT_TRUE(nodes[0] == n2);
  EXPECT_TRUE(bookmark_utils::DoesBookmarkContainText(n2, L"cnn"));
  nodes.clear();

  bookmark_utils::GetBookmarksContainingText(&model, L"foo bar", 100, &nodes);
  ASSERT_EQ(1U, nodes.size());
  EXPECT_TRUE(nodes[0] == n1);
  EXPECT_TRUE(bookmark_utils::DoesBookmarkContainText(n1, L"foo bar"));
  nodes.clear();
}

// Makes sure if the lower case string of a bookmark title is more characters
// than the upper case string no match positions are returned.
TEST_F(BookmarkUtilsTest, EmptyMatchOnMultiwideLowercaseString) {
  BookmarkModel model(NULL);
  BookmarkNode* n1 =
      model.AddURL(model.other_node(), 0, L"\u0130 i",
                   GURL("http://www.google.com"));

  std::vector<bookmark_utils::TitleMatch> matches;
  bookmark_utils::GetBookmarksMatchingText(&model, L"i", 100, &matches);
  ASSERT_EQ(1U, matches.size());
  EXPECT_TRUE(matches[0].node == n1);
  EXPECT_TRUE(matches[0].match_positions.empty());
}
