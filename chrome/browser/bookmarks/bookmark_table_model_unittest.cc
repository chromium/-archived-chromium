// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_util.h"
#include "base/time.h"
#include "chrome/browser/bookmarks/bookmark_table_model.h"
#include "chrome/test/testing_profile.h"
#include "grit/generated_resources.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;
using base::TimeDelta;

// Base class for bookmark model tests.
// Initial state of the bookmark model is as follows:
// bb
//   url1 (t0)
//   f1
// o
//   url2 (t0 + 2)
//   f2
//   url3 (t0 + 1)
class BookmarkTableModelTest : public testing::Test,
                               public views::TableModelObserver {
 public:
  BookmarkTableModelTest()
      : url1_("http://1"),
        url2_("http://2"),
        url3_("http://3"),
        changed_count_(0),
        item_changed_count_(0),
        added_count_(0),
        removed_count_(0) {
   }

  virtual void SetUp() {
    profile_.reset(new TestingProfile());
    profile_->CreateBookmarkModel(true);
    // Populate with some default data.
    Time t0 = Time::Now();
    BookmarkNode* bb = bookmark_model()->GetBookmarkBarNode();
    bookmark_model()->AddURLWithCreationTime(bb, 0, L"a", url1_, t0);
    bookmark_model()->AddGroup(bb, 1, L"f1");

    BookmarkNode* other = bookmark_model()->other_node();
    bookmark_model()->AddURLWithCreationTime(other, 0, L"b",
        url2_, t0 + TimeDelta::FromDays(2));
    bookmark_model()->AddGroup(other, 1, L"f2");
    bookmark_model()->AddURLWithCreationTime(other, 2, L"c", url3_,
        t0 + TimeDelta::FromDays(1));
  }

  virtual void TearDown() {
    model_.reset(NULL);
    profile_.reset(NULL);
  }

  BookmarkModel* bookmark_model() const {
    return profile_->GetBookmarkModel();
  }

  virtual void OnModelChanged() {
    changed_count_++;
  }

  virtual void OnItemsChanged(int start, int length) {
    item_changed_count_++;
  }

  virtual void OnItemsAdded(int start, int length) {
    added_count_++;
  }

  virtual void OnItemsRemoved(int start, int length) {
    removed_count_++;
  }

  void VerifyAndClearOberserverCounts(int changed_count, int item_changed_count,
                                      int added_count, int removed_count) {
    EXPECT_EQ(changed_count, changed_count_);
    EXPECT_EQ(item_changed_count, item_changed_count_);
    EXPECT_EQ(added_count, added_count_);
    EXPECT_EQ(removed_count, removed_count_);
    ResetCounts();
  }

  void ResetCounts() {
    changed_count_ = item_changed_count_ = removed_count_ = added_count_ = 0;
  }

  void SetModel(BookmarkTableModel* model) {
    if (model_.get())
      model_->SetObserver(NULL);
    model_.reset(model);
    if (model_.get())
      model_->SetObserver(this);
  }

  scoped_ptr<BookmarkTableModel> model_;

  const GURL url1_;
  const GURL url2_;
  const GURL url3_;

 private:
  int changed_count_;
  int item_changed_count_;
  int added_count_;
  int removed_count_;
  scoped_ptr<TestingProfile> profile_;
};

// Verifies the count when showing various nodes.
TEST_F(BookmarkTableModelTest, FolderInitialState) {
  SetModel(BookmarkTableModel::CreateBookmarkTableModelForFolder(
      bookmark_model(), bookmark_model()->GetBookmarkBarNode()));
  ASSERT_EQ(2, model_->RowCount());
  EXPECT_TRUE(bookmark_model()->GetBookmarkBarNode()->GetChild(0) ==
              model_->GetNodeForRow(0));
  EXPECT_TRUE(bookmark_model()->GetBookmarkBarNode()->GetChild(1) ==
              model_->GetNodeForRow(1));

  SetModel(BookmarkTableModel::CreateBookmarkTableModelForFolder(
      bookmark_model(), bookmark_model()->other_node()));
  EXPECT_EQ(3, model_->RowCount());
}

// Verifies adding an item to folder model generates the correct event.
TEST_F(BookmarkTableModelTest, AddToFolder) {
  BookmarkNode* other = bookmark_model()->other_node();
  SetModel(BookmarkTableModel::CreateBookmarkTableModelForFolder(
      bookmark_model(), other));
  BookmarkNode* new_node = bookmark_model()->AddURL(other, 0, L"new", url1_);
  // Should have gotten notification of the add.
  VerifyAndClearOberserverCounts(0, 0, 1, 0);
  ASSERT_EQ(4, model_->RowCount());
  EXPECT_TRUE(new_node == model_->GetNodeForRow(0));

  // Add to the bookmark bar, this shouldn't generate an event.
  bookmark_model()->AddURL(bookmark_model()->GetBookmarkBarNode(), 0, L"new",
                           url1_);
  VerifyAndClearOberserverCounts(0, 0, 0, 0);
}

// Verifies removing an item from folder model generates the correct event.
TEST_F(BookmarkTableModelTest, RemoveFromFolder) {
  BookmarkNode* other = bookmark_model()->other_node();
  SetModel(BookmarkTableModel::CreateBookmarkTableModelForFolder(
      bookmark_model(), other));
  bookmark_model()->Remove(other, 0);

  // Should have gotten notification of the remove.
  VerifyAndClearOberserverCounts(0, 0, 0, 1);
  EXPECT_EQ(2, model_->RowCount());

  // Remove from the bookmark bar, this shouldn't generate an event.
  bookmark_model()->Remove(bookmark_model()->GetBookmarkBarNode(), 0);
  VerifyAndClearOberserverCounts(0, 0, 0, 0);
}

// Verifies changing an item in the folder model generates the correct event.
TEST_F(BookmarkTableModelTest, ChangeFolder) {
  BookmarkNode* other = bookmark_model()->other_node();
  SetModel(BookmarkTableModel::CreateBookmarkTableModelForFolder(
      bookmark_model(), other));
  bookmark_model()->SetTitle(other->GetChild(0), L"new");

  // Should have gotten notification of the change.
  VerifyAndClearOberserverCounts(0, 1, 0, 0);
  EXPECT_EQ(3, model_->RowCount());

  // Change a node in the bookmark bar, this shouldn't generate an event.
  bookmark_model()->SetTitle(
      bookmark_model()->GetBookmarkBarNode()->GetChild(0), L"new2");
  VerifyAndClearOberserverCounts(0, 0, 0, 0);
}

// Verifies show recently added shows the recently added, in order.
TEST_F(BookmarkTableModelTest, RecentlyBookmarkedOrder) {
  SetModel(BookmarkTableModel::CreateRecentlyBookmarkedModel(bookmark_model()));
  EXPECT_EQ(3, model_->RowCount());

  BookmarkNode* bb = bookmark_model()->GetBookmarkBarNode();
  BookmarkNode* other = bookmark_model()->other_node();
  EXPECT_TRUE(other->GetChild(0) == model_->GetNodeForRow(0));
  EXPECT_TRUE(other->GetChild(2) == model_->GetNodeForRow(1));
  EXPECT_TRUE(bb->GetChild(0) == model_->GetNodeForRow(2));
}

// Verifies adding an item to recently added notifies observer.
TEST_F(BookmarkTableModelTest, AddToRecentlyBookmarked) {
  SetModel(BookmarkTableModel::CreateRecentlyBookmarkedModel(bookmark_model()));
  bookmark_model()->AddURL(bookmark_model()->other_node(), 0, L"new", url1_);
  // Should have gotten notification of the add.
  VerifyAndClearOberserverCounts(1, 0, 0, 0);
  EXPECT_EQ(4, model_->RowCount());

  // Add a folder, this shouldn't change the model.
  bookmark_model()->AddGroup(bookmark_model()->GetBookmarkBarNode(), 0, L"new");
  VerifyAndClearOberserverCounts(0, 0, 0, 0);
}

// Verifies removing an item from recently added notifies observer.
TEST_F(BookmarkTableModelTest, RemoveFromRecentlyBookmarked) {
  SetModel(BookmarkTableModel::CreateRecentlyBookmarkedModel(bookmark_model()));
  bookmark_model()->Remove(bookmark_model()->other_node(), 0);
  // Should have gotten notification of the remove.
  VerifyAndClearOberserverCounts(1, 0, 0, 0);
  EXPECT_EQ(2, model_->RowCount());

  // Remove a folder, this shouldn't change the model.
  bookmark_model()->Remove(bookmark_model()->other_node(), 0);
  VerifyAndClearOberserverCounts(0, 0, 0, 0);
}

// Verifies changing an item in recently added notifies observer.
TEST_F(BookmarkTableModelTest, ChangeRecentlyBookmarked) {
  SetModel(BookmarkTableModel::CreateRecentlyBookmarkedModel(bookmark_model()));
  bookmark_model()->SetTitle(bookmark_model()->other_node()->GetChild(0),
                             L"new");
  // Should have gotten notification of the change.
  VerifyAndClearOberserverCounts(0, 1, 0, 0);

  // Change a folder, this shouldn't change the model.
  bookmark_model()->SetTitle(bookmark_model()->other_node()->GetChild(1),
                             L"new");
  VerifyAndClearOberserverCounts(0, 0, 0, 0);
}

// Verifies search finds the correct bookmarks.
TEST_F(BookmarkTableModelTest, Search) {
  SetModel(BookmarkTableModel::CreateSearchTableModel(bookmark_model(), L"c"));
  ASSERT_EQ(1, model_->RowCount());
  EXPECT_TRUE(bookmark_model()->other_node()->GetChild(2) ==
              model_->GetNodeForRow(0));
  // Make sure IndexOfNode works.
  EXPECT_EQ(0,
            model_->IndexOfNode(bookmark_model()->other_node()->GetChild(2)));
}

// Verifies adding an item to search notifies observers.
TEST_F(BookmarkTableModelTest, AddToSearch) {
  SetModel(BookmarkTableModel::CreateSearchTableModel(bookmark_model(), L"c"));
  BookmarkNode* new_node =
      bookmark_model()->AddURL(bookmark_model()->other_node(), 0, L"c", url1_);
  // Should have gotten notification of the add.
  VerifyAndClearOberserverCounts(0, 0, 1, 0);
  ASSERT_EQ(2, model_->RowCount());
  // New node should have gone to end.
  EXPECT_TRUE(model_->GetNodeForRow(1) == new_node);

  // Add a folder, this shouldn't change the model.
  bookmark_model()->AddGroup(bookmark_model()->GetBookmarkBarNode(), 0, L"new");
  VerifyAndClearOberserverCounts(0, 0, 0, 0);
  EXPECT_EQ(2, model_->RowCount());

  // Add a url that doesn't match search, this shouldn't change the model.
  bookmark_model()->AddGroup(bookmark_model()->GetBookmarkBarNode(), 0, L"new");
  VerifyAndClearOberserverCounts(0, 0, 0, 0);
  EXPECT_EQ(2, model_->RowCount());
}

// Verifies removing an item updates search.
TEST_F(BookmarkTableModelTest, RemoveFromSearch) {
  SetModel(BookmarkTableModel::CreateSearchTableModel(bookmark_model(), L"c"));
  bookmark_model()->Remove(bookmark_model()->other_node(), 2);
  // Should have gotten notification of the remove.
  VerifyAndClearOberserverCounts(0, 0, 0, 1);
  EXPECT_EQ(0, model_->RowCount());

  // Remove a folder, this shouldn't change the model.
  bookmark_model()->Remove(bookmark_model()->other_node(), 1);
  VerifyAndClearOberserverCounts(0, 0, 0, 0);

  // Remove another url that isn't in the model, this shouldn't change anything.
  bookmark_model()->Remove(bookmark_model()->other_node(), 0);
  VerifyAndClearOberserverCounts(0, 0, 0, 0);
}

// Verifies changing an item in search notifies observer.
TEST_F(BookmarkTableModelTest, ChangeSearch) {
  SetModel(BookmarkTableModel::CreateSearchTableModel(bookmark_model(), L"c"));
  bookmark_model()->SetTitle(bookmark_model()->other_node()->GetChild(2),
                             L"new");
  // Should have gotten notification of the change.
  VerifyAndClearOberserverCounts(0, 1, 0, 0);

  // Change a folder, this shouldn't change the model.
  bookmark_model()->SetTitle(bookmark_model()->other_node()->GetChild(1),
                             L"new");
  VerifyAndClearOberserverCounts(0, 0, 0, 0);

  // Change a url that isn't in the model, this shouldn't send change.
  bookmark_model()->SetTitle(bookmark_model()->other_node()->GetChild(0),
                             L"new");
  VerifyAndClearOberserverCounts(0, 0, 0, 0);
}
