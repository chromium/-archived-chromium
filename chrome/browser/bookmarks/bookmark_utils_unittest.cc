// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"

typedef testing::Test BookmarkUtilsTest;

TEST_F(BookmarkUtilsTest, GetBookmarksContainingText) {
  BookmarkModel model(NULL);
  const BookmarkNode* n1 =
      model.AddURL(model.other_node(), 0, L"foo bar",
                   GURL("http://www.google.com"));
  const BookmarkNode* n2 =
      model.AddURL(model.other_node(), 0, L"baz buz",
                   GURL("http://www.cnn.com"));

  model.AddGroup(model.other_node(), 0, L"foo");

  std::vector<const BookmarkNode*> nodes;
  bookmark_utils::GetBookmarksContainingText(
      &model, L"foo", 100, std::wstring(), &nodes);
  ASSERT_EQ(1U, nodes.size());
  EXPECT_TRUE(nodes[0] == n1);
  EXPECT_TRUE(bookmark_utils::DoesBookmarkContainText(
      n1, L"foo", std::wstring()));
  nodes.clear();

  bookmark_utils::GetBookmarksContainingText(
      &model, L"cnn", 100, std::wstring(), &nodes);
  ASSERT_EQ(1U, nodes.size());
  EXPECT_TRUE(nodes[0] == n2);
  EXPECT_TRUE(bookmark_utils::DoesBookmarkContainText(
      n2, L"cnn", std::wstring()));
  nodes.clear();

  bookmark_utils::GetBookmarksContainingText(
      &model, L"foo bar", 100, std::wstring(), &nodes);
  ASSERT_EQ(1U, nodes.size());
  EXPECT_TRUE(nodes[0] == n1);
  EXPECT_TRUE(bookmark_utils::DoesBookmarkContainText(
      n1, L"foo bar", std::wstring()));
  nodes.clear();
}

TEST_F(BookmarkUtilsTest, DoesBookmarkContainText) {
  BookmarkModel model(NULL);
  const BookmarkNode* node = model.AddURL(model.other_node(), 0, L"foo bar",
                                          GURL("http://www.google.com"));
  // Matches to the title.
  ASSERT_TRUE(bookmark_utils::DoesBookmarkContainText(
      node, L"ar", std::wstring()));
  // Matches to the URL
  ASSERT_TRUE(bookmark_utils::DoesBookmarkContainText(
      node, L"www", std::wstring()));
  // No match.
  ASSERT_FALSE(bookmark_utils::DoesBookmarkContainText(
      node, L"cnn", std::wstring()));

  // Tests for a Japanese IDN.
  const wchar_t* kDecodedIdn = L"\x30B0\x30FC\x30B0\x30EB";
  node = model.AddURL(model.other_node(), 0, L"foo bar",
                      GURL("http://xn--qcka1pmc.jp"));
  // Unicode query doesn't match if languages have no "ja".
  ASSERT_FALSE(bookmark_utils::DoesBookmarkContainText(
      node, kDecodedIdn, L"en"));
  // Unicode query matches if languages have "ja".
  ASSERT_TRUE(bookmark_utils::DoesBookmarkContainText(
      node, kDecodedIdn, L"ja"));
  // Punycode query also matches as ever.
  ASSERT_TRUE(bookmark_utils::DoesBookmarkContainText(
      node, L"qcka1pmc", L"ja"));
}

