// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_ptr.h"
#include "chrome/browser/bookmarks/bookmark_drag_data.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/common/os_exchange_data.h"
#include "chrome/test/testing_profile.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

typedef testing::Test BookmarkDragDataTest;

TEST_F(BookmarkDragDataTest, InitialState) {
  BookmarkDragData data;
  EXPECT_FALSE(data.is_valid);
}

TEST_F(BookmarkDragDataTest, BogusRead) {
  scoped_refptr<OSExchangeData> data(new OSExchangeData());
  BookmarkDragData drag_data;
  drag_data.is_valid = true;
  EXPECT_FALSE(drag_data.Read(data.get()));
  EXPECT_FALSE(drag_data.is_valid);
}

TEST_F(BookmarkDragDataTest, URL) {
  TestingProfile profile;
  profile.CreateBookmarkModel(false);
  profile.SetID(L"id");
  BookmarkModel* model = profile.GetBookmarkModel();
  BookmarkNode* root = model->GetBookmarkBarNode();
  GURL url(GURL("http://foo.com"));
  const std::wstring title(L"blah");
  BookmarkNode* node = model->AddURL(root, 0, title, url);
  BookmarkDragData drag_data(node);
  EXPECT_TRUE(drag_data.url == url);
  EXPECT_EQ(title, drag_data.title);
  EXPECT_TRUE(drag_data.is_url);

  scoped_refptr<OSExchangeData> data(new OSExchangeData());
  drag_data.Write(&profile, data.get());

  // Now read the data back in.
  scoped_refptr<OSExchangeData> data2(new OSExchangeData(data.get()));
  BookmarkDragData read_data;
  EXPECT_TRUE(read_data.Read(*data2));
  EXPECT_TRUE(read_data.url == url);
  EXPECT_EQ(title, read_data.title);
  EXPECT_TRUE(read_data.is_valid);
  EXPECT_TRUE(read_data.is_url);
  EXPECT_TRUE(read_data.GetNode(&profile) == node);

  TestingProfile profile2(1);
  EXPECT_TRUE(read_data.GetNode(&profile2) == NULL);

  // Writing should also put the URL and title on the clipboard.
  GURL read_url;
  std::wstring read_title;
  EXPECT_TRUE(data2->GetURLAndTitle(&read_url, &read_title));
  EXPECT_TRUE(read_url == url);
  EXPECT_EQ(title, read_title);
}

TEST_F(BookmarkDragDataTest, Group) {
  TestingProfile profile;
  profile.CreateBookmarkModel(false);
  profile.SetID(L"id");
  BookmarkModel* model = profile.GetBookmarkModel();
  BookmarkNode* root = model->GetBookmarkBarNode();
  BookmarkNode* g1 = model->AddGroup(root, 0, L"g1");
  BookmarkNode* g11 = model->AddGroup(g1, 0, L"g11");
  BookmarkNode* g12 = model->AddGroup(g1, 0, L"g12");

  BookmarkDragData drag_data(g12);
  EXPECT_EQ(g12->GetTitle(), drag_data.title);
  EXPECT_FALSE(drag_data.is_url);

  scoped_refptr<OSExchangeData> data(new OSExchangeData());
  drag_data.Write(&profile, data.get());

  // Now read the data back in.
  scoped_refptr<OSExchangeData> data2(new OSExchangeData(data.get()));
  BookmarkDragData read_data;
  EXPECT_TRUE(read_data.Read(*data2));
  EXPECT_EQ(g12->GetTitle(), read_data.title);
  EXPECT_TRUE(read_data.is_valid);
  EXPECT_FALSE(read_data.is_url);

  BookmarkNode* r_g12 = read_data.GetNode(&profile);
  EXPECT_TRUE(g12 == r_g12);

  TestingProfile profile2(1);
  EXPECT_TRUE(read_data.GetNode(&profile2) == NULL);
}

TEST_F(BookmarkDragDataTest, GroupWithChild) {
  TestingProfile profile;
  profile.SetID(L"id");
  profile.CreateBookmarkModel(false);
  BookmarkModel* model = profile.GetBookmarkModel();
  BookmarkNode* root = model->GetBookmarkBarNode();
  BookmarkNode* group = model->AddGroup(root, 0, L"g1");

  GURL url(GURL("http://foo.com"));
  const std::wstring title(L"blah2");

  model->AddURL(group, 0, title, url);

  BookmarkDragData drag_data(group);

  scoped_refptr<OSExchangeData> data(new OSExchangeData());
  drag_data.Write(&profile, data.get());

  // Now read the data back in.
  scoped_refptr<OSExchangeData> data2(new OSExchangeData(data.get()));
  BookmarkDragData read_data;
  EXPECT_TRUE(read_data.Read(*data2));

  EXPECT_EQ(1, read_data.children.size());
  EXPECT_TRUE(read_data.children[0].is_valid);
  EXPECT_TRUE(read_data.children[0].is_url);
  EXPECT_EQ(title, read_data.children[0].title);
  EXPECT_TRUE(url == read_data.children[0].url);
  EXPECT_TRUE(read_data.children[0].is_url);

  BookmarkNode* r_group = read_data.GetNode(&profile);
  EXPECT_TRUE(group == r_group);
}
