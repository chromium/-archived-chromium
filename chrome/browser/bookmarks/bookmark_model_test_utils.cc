// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_model_test_utils.h"

#include "chrome/browser/bookmarks/bookmark_model.h"
#include "testing/gtest/include/gtest/gtest.h"

// static
void BookmarkModelTestUtils::AssertNodesEqual(const BookmarkNode* expected,
                                              const BookmarkNode* actual,
                                              bool check_ids) {
  ASSERT_TRUE(expected);
  ASSERT_TRUE(actual);
  if (check_ids)
    EXPECT_EQ(expected->id(), actual->id());
  EXPECT_EQ(expected->GetTitle(), actual->GetTitle());
  EXPECT_EQ(expected->GetType(), actual->GetType());
  EXPECT_TRUE(expected->date_added() == actual->date_added());
  if (expected->GetType() == history::StarredEntry::URL) {
    EXPECT_EQ(expected->GetURL(), actual->GetURL());
  } else {
    EXPECT_TRUE(expected->date_group_modified() ==
                actual->date_group_modified());
    ASSERT_EQ(expected->GetChildCount(), actual->GetChildCount());
    for (int i = 0; i < expected->GetChildCount(); ++i)
      AssertNodesEqual(expected->GetChild(i), actual->GetChild(i), check_ids);
  }
}

// static
void BookmarkModelTestUtils::AssertModelsEqual(BookmarkModel* expected,
                                               BookmarkModel* actual,
                                               bool check_ids) {
  AssertNodesEqual(expected->GetBookmarkBarNode(),
                   actual->GetBookmarkBarNode(),
                   check_ids);
  AssertNodesEqual(expected->other_node(),
                   actual->other_node(),
                   check_ids);
}

