// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_BOOKMARK_MODEL_TEST_UTILS_H_
#define CHROME_BROWSER_BOOKMARKS_BOOKMARK_MODEL_TEST_UTILS_H_

class BookmarkModel;
class BookmarkNode;

// Contains utilities for bookmark model related tests.
class BookmarkModelTestUtils {
 public:
  // Verifies that the two given bookmark models are the same.
  // The IDs of the bookmark nodes are compared only if check_ids is true.
  static void AssertModelsEqual(BookmarkModel* expected,
                                BookmarkModel* actual,
                                bool check_ids);
 private:
  // Helper to verify the two given bookmark nodes.
  // The IDs of the bookmark nodes are compared only if check_ids is true.
  static void AssertNodesEqual(const BookmarkNode* expected,
                               const BookmarkNode* actual,
                               bool check_ids);
};

#endif  // CHROME_BROWSER_BOOKMARKS_BOOKMARK_MODEL_TEST_UTILS_H_
