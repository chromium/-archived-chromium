// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/os_exchange_data.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/bookmarks/bookmark_drag_data.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/test/testing_profile.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

typedef testing::Test BookmarkDragDataTest;

// Makes sure BookmarkDragData is initially invalid.
TEST_F(BookmarkDragDataTest, InitialState) {
  BookmarkDragData data;
  EXPECT_FALSE(data.is_valid());
}

// Makes sure reading bogus data leaves the BookmarkDragData invalid.
TEST_F(BookmarkDragDataTest, BogusRead) {
  scoped_refptr<OSExchangeData> data(new OSExchangeData());
  BookmarkDragData drag_data;
  EXPECT_FALSE(drag_data.Read(OSExchangeData(data.get())));
  EXPECT_FALSE(drag_data.is_valid());
}

// Writes a URL to the clipboard and make sure BookmarkDragData can correctly
// read it.
TEST_F(BookmarkDragDataTest, JustURL) {
  const GURL url("http://google.com");
  const std::wstring title(L"title");

  scoped_refptr<OSExchangeData> data(new OSExchangeData());
  data->SetURL(url, title);

  BookmarkDragData drag_data;
  EXPECT_TRUE(drag_data.Read(OSExchangeData(data.get())));
  EXPECT_TRUE(drag_data.is_valid());
  ASSERT_EQ(1, drag_data.elements.size());
  EXPECT_TRUE(drag_data.elements[0].is_url);
  EXPECT_TRUE(drag_data.elements[0].url == url);
  EXPECT_TRUE(drag_data.elements[0].title == title);
  EXPECT_EQ(0, drag_data.elements[0].children.size());
}

TEST_F(BookmarkDragDataTest, URL) {
  // Write a single node representing a URL to the clipboard.
  TestingProfile profile;
  profile.CreateBookmarkModel(false);
  profile.SetID(L"id");
  BookmarkModel* model = profile.GetBookmarkModel();
  const BookmarkNode* root = model->GetBookmarkBarNode();
  GURL url(GURL("http://foo.com"));
  const std::wstring title(L"blah");
  const BookmarkNode* node = model->AddURL(root, 0, title, url);
  BookmarkDragData drag_data(node);
  EXPECT_TRUE(drag_data.is_valid());
  ASSERT_EQ(1, drag_data.elements.size());
  EXPECT_TRUE(drag_data.elements[0].is_url);
  EXPECT_TRUE(drag_data.elements[0].url == url);
  EXPECT_EQ(title, drag_data.elements[0].title);
  scoped_refptr<OSExchangeData> data(new OSExchangeData());
  drag_data.Write(&profile, data.get());

  // Now read the data back in.
  scoped_refptr<OSExchangeData> data2(new OSExchangeData(data.get()));
  BookmarkDragData read_data;
  EXPECT_TRUE(read_data.Read(*data2));
  EXPECT_TRUE(read_data.is_valid());
  ASSERT_EQ(1, read_data.elements.size());
  EXPECT_TRUE(read_data.elements[0].is_url);
  EXPECT_TRUE(read_data.elements[0].url == url);
  EXPECT_EQ(title, read_data.elements[0].title);
  EXPECT_TRUE(read_data.GetFirstNode(&profile) == node);

  // Make sure asking for the node with a different profile returns NULL.
  TestingProfile profile2(1);
  EXPECT_TRUE(read_data.GetFirstNode(&profile2) == NULL);

  // Writing should also put the URL and title on the clipboard.
  GURL read_url;
  std::wstring read_title;
  EXPECT_TRUE(data2->GetURLAndTitle(&read_url, &read_title));
  EXPECT_TRUE(read_url == url);
  EXPECT_EQ(title, read_title);
}

// Tests writing a group to the clipboard.
TEST_F(BookmarkDragDataTest, Group) {
  TestingProfile profile;
  profile.CreateBookmarkModel(false);
  profile.SetID(L"id");
  BookmarkModel* model = profile.GetBookmarkModel();
  const BookmarkNode* root = model->GetBookmarkBarNode();
  const BookmarkNode* g1 = model->AddGroup(root, 0, L"g1");
  const BookmarkNode* g11 = model->AddGroup(g1, 0, L"g11");
  const BookmarkNode* g12 = model->AddGroup(g1, 0, L"g12");

  BookmarkDragData drag_data(g12);
  EXPECT_TRUE(drag_data.is_valid());
  ASSERT_EQ(1, drag_data.elements.size());
  EXPECT_EQ(g12->GetTitle(), drag_data.elements[0].title);
  EXPECT_FALSE(drag_data.elements[0].is_url);

  scoped_refptr<OSExchangeData> data(new OSExchangeData());
  drag_data.Write(&profile, data.get());

  // Now read the data back in.
  scoped_refptr<OSExchangeData> data2(new OSExchangeData(data.get()));
  BookmarkDragData read_data;
  EXPECT_TRUE(read_data.Read(*data2));
  EXPECT_TRUE(read_data.is_valid());
  ASSERT_EQ(1, read_data.elements.size());
  EXPECT_EQ(g12->GetTitle(), read_data.elements[0].title);
  EXPECT_FALSE(read_data.elements[0].is_url);

  // We should get back the same node when asking for the same profile.
  const BookmarkNode* r_g12 = read_data.GetFirstNode(&profile);
  EXPECT_TRUE(g12 == r_g12);

  // A different profile should return NULL for the node.
  TestingProfile profile2(1);
  EXPECT_TRUE(read_data.GetFirstNode(&profile2) == NULL);
}

// Tests reading/writing a folder with children.
TEST_F(BookmarkDragDataTest, GroupWithChild) {
  TestingProfile profile;
  profile.SetID(L"id");
  profile.CreateBookmarkModel(false);
  BookmarkModel* model = profile.GetBookmarkModel();
  const BookmarkNode* root = model->GetBookmarkBarNode();
  const BookmarkNode* group = model->AddGroup(root, 0, L"g1");

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
  ASSERT_EQ(1, read_data.elements.size());
  ASSERT_EQ(1, read_data.elements[0].children.size());
  const BookmarkDragData::Element& read_child =
      read_data.elements[0].children[0];

  EXPECT_TRUE(read_child.is_url);
  EXPECT_EQ(title, read_child.title);
  EXPECT_TRUE(url == read_child.url);
  EXPECT_TRUE(read_child.is_url);

  // And make sure we get the node back.
  const BookmarkNode* r_group = read_data.GetFirstNode(&profile);
  EXPECT_TRUE(group == r_group);
}

// Tests reading/writing of multiple nodes.
TEST_F(BookmarkDragDataTest, MultipleNodes) {
  TestingProfile profile;
  profile.SetID(L"id");
  profile.CreateBookmarkModel(false);
  BookmarkModel* model = profile.GetBookmarkModel();
  const BookmarkNode* root = model->GetBookmarkBarNode();
  const BookmarkNode* group = model->AddGroup(root, 0, L"g1");

  GURL url(GURL("http://foo.com"));
  const std::wstring title(L"blah2");

  const BookmarkNode* url_node = model->AddURL(group, 0, title, url);

  // Write the nodes to the clipboard.
  std::vector<const BookmarkNode*> nodes;
  nodes.push_back(group);
  nodes.push_back(url_node);
  BookmarkDragData drag_data(nodes);
  scoped_refptr<OSExchangeData> data(new OSExchangeData());
  drag_data.Write(&profile, data.get());

  // Read the data back in.
  scoped_refptr<OSExchangeData> data2(new OSExchangeData(data.get()));
  BookmarkDragData read_data;
  EXPECT_TRUE(read_data.Read(*data2));
  EXPECT_TRUE(read_data.is_valid());
  ASSERT_EQ(2, read_data.elements.size());
  ASSERT_EQ(1, read_data.elements[0].children.size());

  const BookmarkDragData::Element& read_group = read_data.elements[0];
  EXPECT_FALSE(read_group.is_url);
  EXPECT_EQ(L"g1", read_group.title);
  EXPECT_EQ(1, read_group.children.size());

  const BookmarkDragData::Element& read_url = read_data.elements[1];
  EXPECT_TRUE(read_url.is_url);
  EXPECT_EQ(title, read_url.title);
  EXPECT_EQ(0, read_url.children.size());

  // And make sure we get the node back.
  std::vector<const BookmarkNode*> read_nodes = read_data.GetNodes(&profile);
  ASSERT_EQ(2, read_nodes.size());
  EXPECT_TRUE(read_nodes[0] == group);
  EXPECT_TRUE(read_nodes[1] == url_node);

  // Asking for the first node should return NULL with more than one element
  // present.
  EXPECT_TRUE(read_data.GetFirstNode(&profile) == NULL);
}
