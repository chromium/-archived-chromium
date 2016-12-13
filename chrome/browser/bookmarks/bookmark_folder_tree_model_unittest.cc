// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_util.h"
#include "chrome/browser/bookmarks/bookmark_folder_tree_model.h"
#include "chrome/test/testing_profile.h"
#include "grit/generated_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "views/controls/tree/tree_view.h"

// Base class for bookmark model tests.
// Initial state of the bookmark model is as follows:
// bb
//   url1
//   f1
//     f11
// o
//   url2
//   f2
//   url3
//   a1
//   g1
class BookmarkFolderTreeModelTest : public testing::Test,
                                    public TreeModelObserver {
 public:
  BookmarkFolderTreeModelTest()
      : url1_("http://1"),
        url2_("http://2"),
        url3_("http://3"),
        added_count_(0),
        removed_count_(0),
        changed_count_(0),
        reordered_count_(0) {
   }

  virtual void SetUp() {
    profile_.reset(new TestingProfile());
    profile_->CreateBookmarkModel(true);
    // Populate with some default data.
    const BookmarkNode* bb = bookmark_model()->GetBookmarkBarNode();
    bookmark_model()->AddURL(bb, 0, L"url1", url1_);
    const BookmarkNode* f1 = bookmark_model()->AddGroup(bb, 1, L"f1");
    bookmark_model()->AddGroup(f1, 0, L"f11");

    const BookmarkNode* other = bookmark_model()->other_node();
    bookmark_model()->AddURL(other, 0, L"url2", url2_);
    bookmark_model()->AddGroup(other, 1, L"f2");
    bookmark_model()->AddURL(other, 2, L"url3", url3_);

    model_.reset(new BookmarkFolderTreeModel(bookmark_model()));
    model_->SetObserver(this);
  }

  virtual void TearDown() {
    model_.reset(NULL);
    profile_.reset(NULL);
  }

  BookmarkModel* bookmark_model() const {
    return profile_->GetBookmarkModel();
  }

  virtual void TreeNodesAdded(TreeModel* model,
                              TreeModelNode* parent,
                              int start,
                              int count) {
    added_count_++;
  }

  virtual void TreeNodesRemoved(TreeModel* model,
                                TreeModelNode* parent,
                                int start,
                                int count) {
    removed_count_++;
  }

  virtual void TreeNodeChanged(TreeModel* model,
                               TreeModelNode* node) {
    changed_count_++;
  }

  virtual void TreeNodeChildrenReordered(TreeModel* model,
                                         TreeModelNode* parent) {
    reordered_count_++;
  }

  void VerifyAndClearObserverCounts(int changed_count,
                                    int added_count,
                                    int removed_count,
                                    int reordered_count) {
    EXPECT_EQ(changed_count, changed_count_);
    EXPECT_EQ(added_count, added_count_);
    EXPECT_EQ(removed_count, removed_count_);
    EXPECT_EQ(reordered_count, reordered_count_);
    ResetCounts();
  }

  void ResetCounts() {
    changed_count_ = removed_count_ = added_count_ = reordered_count_ = 0;
  }

  scoped_ptr<BookmarkFolderTreeModel> model_;

  const GURL url1_;
  const GURL url2_;
  const GURL url3_;

 private:
  int changed_count_;
  int added_count_;
  int removed_count_;
  int reordered_count_;
  scoped_ptr<TestingProfile> profile_;
};

// Verifies the root node has 4 nodes, and the contents of the bookmark bar
// and other folders matches the initial state.
TEST_F(BookmarkFolderTreeModelTest, InitialState) {
  // Verify the first 4 nodes.
  TreeModelNode* root = model_->GetRoot();
  ASSERT_EQ(4, model_->GetChildCount(root));
  EXPECT_EQ(BookmarkFolderTreeModel::BOOKMARK,
            model_->GetNodeType(model_->GetChild(root, 0)));
  EXPECT_EQ(BookmarkFolderTreeModel::BOOKMARK,
            model_->GetNodeType(model_->GetChild(root, 1)));
  EXPECT_EQ(BookmarkFolderTreeModel::RECENTLY_BOOKMARKED,
            model_->GetNodeType(model_->GetChild(root, 2)));
  EXPECT_EQ(BookmarkFolderTreeModel::SEARCH,
            model_->GetNodeType(model_->GetChild(root, 3)));

  // Verify the contents of the bookmark bar node.
  TreeModelNode* bb_node = model_->GetChild(root, 0);
  EXPECT_TRUE(model_->TreeNodeAsBookmarkNode(bb_node) ==
              bookmark_model()->GetBookmarkBarNode());
  ASSERT_EQ(1, model_->GetChildCount(bb_node));
  EXPECT_TRUE(model_->TreeNodeAsBookmarkNode(model_->GetChild(bb_node, 0)) ==
              bookmark_model()->GetBookmarkBarNode()->GetChild(1));

  // Verify the contents of the other folders node.
  TreeModelNode* other_node = model_->GetChild(root, 1);
  EXPECT_TRUE(model_->TreeNodeAsBookmarkNode(other_node) ==
              bookmark_model()->other_node());
  ASSERT_EQ(1, model_->GetChildCount(other_node));
  EXPECT_TRUE(model_->TreeNodeAsBookmarkNode(model_->GetChild(other_node, 0)) ==
              bookmark_model()->other_node()->GetChild(1));
}

// Removes a URL node and makes sure we don't get any notification.
TEST_F(BookmarkFolderTreeModelTest, RemoveURL) {
  bookmark_model()->Remove(bookmark_model()->GetBookmarkBarNode(), 0);
  VerifyAndClearObserverCounts(0, 0, 0, 0);
}

// Changes the title of a URL and makes sure we don't get any notification.
TEST_F(BookmarkFolderTreeModelTest, ChangeURL) {
  bookmark_model()->SetTitle(
      bookmark_model()->GetBookmarkBarNode()->GetChild(0), L"BLAH");
  VerifyAndClearObserverCounts(0, 0, 0, 0);
}

// Adds a URL and make sure we don't get notification.
TEST_F(BookmarkFolderTreeModelTest, AddURL) {
  bookmark_model()->AddURL(
      bookmark_model()->other_node(), 0, L"url1", url1_);
  VerifyAndClearObserverCounts(0, 0, 0, 0);
}

// Removes a folder and makes sure we get the right notification.
TEST_F(BookmarkFolderTreeModelTest, RemoveFolder) {
  bookmark_model()->Remove(bookmark_model()->GetBookmarkBarNode(), 1);
  VerifyAndClearObserverCounts(0, 0, 1, 0);
  // Make sure the node was removed.
  EXPECT_EQ(0, model_->GetRoot()->GetChild(0)->GetChildCount());
}

// Adds a folder and makes sure we get the right notification.
TEST_F(BookmarkFolderTreeModelTest, AddFolder) {
  const BookmarkNode* new_group =
      bookmark_model()->AddGroup(
          bookmark_model()->GetBookmarkBarNode(), 0, L"fa");
  VerifyAndClearObserverCounts(0, 1, 0, 0);
  // Make sure the node was added at the right place.
  // Make sure the node was removed.
  ASSERT_EQ(2, model_->GetRoot()->GetChild(0)->GetChildCount());
  EXPECT_TRUE(new_group == model_->TreeNodeAsBookmarkNode(
      model_->GetRoot()->GetChild(0)->GetChild(0)));
}

// Changes the title of a folder and makes sure we don't get any notification.
TEST_F(BookmarkFolderTreeModelTest, ChangeFolder) {
  bookmark_model()->SetTitle(
      bookmark_model()->GetBookmarkBarNode()->GetChild(1)->GetChild(0),
      L"BLAH");
  VerifyAndClearObserverCounts(1, 0, 0, 0);
}

// Sorts the other folder, making sure the resulting order is correct and the
// appropriate notification is sent.
TEST_F(BookmarkFolderTreeModelTest, Sort) {
  const BookmarkNode* other = bookmark_model()->other_node();
  bookmark_model()->AddGroup(other, 3, L"a1");
  bookmark_model()->AddGroup(other, 4, L"g1");
  ResetCounts();

  bookmark_model()->SortChildren(other);

  // Make sure we got notification.
  VerifyAndClearObserverCounts(0, 0, 0, 1);

  // Make sure the resulting order matches.
  FolderNode* other_folder_node =
      model_->GetFolderNodeForBookmarkNode(bookmark_model()->other_node());
  ASSERT_EQ(3, other_folder_node->GetChildCount());
  EXPECT_TRUE(other_folder_node->GetChild(0)->GetTitle() == L"a1");
  EXPECT_TRUE(other_folder_node->GetChild(1)->GetTitle() == L"f2");
  EXPECT_TRUE(other_folder_node->GetChild(2)->GetTitle() == L"g1");
}
