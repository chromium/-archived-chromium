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
  ASSERT_EQ(static_cast<size_t>(1), nodes.size());
  EXPECT_TRUE(nodes[0] == n1);
  EXPECT_TRUE(bookmark_utils::DoesBookmarkContainText(n1, L"foo"));
  nodes.clear();

  bookmark_utils::GetBookmarksContainingText(&model, L"cnn", 100, &nodes);
  ASSERT_EQ(static_cast<size_t>(1), nodes.size());
  EXPECT_TRUE(nodes[0] == n2);
  EXPECT_TRUE(bookmark_utils::DoesBookmarkContainText(n2, L"cnn"));
  nodes.clear();

  bookmark_utils::GetBookmarksContainingText(&model, L"foo bar", 100, &nodes);
  ASSERT_EQ(static_cast<size_t>(1), nodes.size());
  EXPECT_TRUE(nodes[0] == n1);
  EXPECT_TRUE(bookmark_utils::DoesBookmarkContainText(n1, L"foo bar"));
  nodes.clear();
}
