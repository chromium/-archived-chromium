// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_util.h"
#include "chrome/browser/bookmark_bar_model.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/views/bookmark_editor_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/test/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

// Base class for bookmark editor tests. Creates a BookmarkBarModel and
// populates it with test data.
class BookmarkEditorViewTest : public testing::Test {
 public:
  BookmarkEditorViewTest() : model_(NULL) {
  }

  virtual void SetUp() {
    profile_.reset(new TestingProfile());
    profile_->set_has_history_service(true);
    profile_->CreateBookmarkBarModel();

    model_ = profile_->GetBookmarkBarModel();

    AddTestData();
  }

  virtual void TearDown() {
  }

 protected:
  BookmarkBarModel* model_;
  scoped_ptr<TestingProfile> profile_;

  std::string base_path() const { return "file:///c:/tmp/"; }

 private:
  // Creates the following structure:
  // bookmark bar node
  //   a
  //   F1
  //    f1a
  //    F11
  //     f11a
  //   F2
  // other node
  //   oa
  //   OF1
  //     of1a
  void AddTestData() {
    std::string test_base = base_path();

    model_->AddURL(model_->GetBookmarkBarNode(), 0, L"a",
                   GURL(test_base + "a"));
    BookmarkBarNode* f1 =
        model_->AddGroup(model_->GetBookmarkBarNode(), 1, L"F1");
    model_->AddURL(f1, 0, L"f1a", GURL(test_base + "f1a"));
    BookmarkBarNode* f11 = model_->AddGroup(f1, 1, L"F11");
    model_->AddURL(f11, 0, L"f11a", GURL(test_base + "f11a"));
    model_->AddGroup(model_->GetBookmarkBarNode(), 2, L"F2");

    // Children of the other node.
    model_->AddURL(model_->other_node(), 0, L"oa",
                   GURL(test_base + "oa"));
    BookmarkBarNode* of1 =
        model_->AddGroup(model_->other_node(), 1, L"OF1");
    model_->AddURL(of1, 0, L"of1a", GURL(test_base + "of1a"));
  }
};

// Makes sure the tree model matches that of the bookmark bar model.
TEST_F(BookmarkEditorViewTest, ModelsMatch) {
  BookmarkEditorView editor(profile_.get(), GURL(base_path() + "xxx"),
                            L"xxx");
  BookmarkEditorView::BookmarkNode* editor_root = editor.tree_model_->GetRoot();
  // The root should have two children, one for the bookmark bar node,
  // the other for the 'other bookmarks' folder.
  ASSERT_EQ(2, editor_root->GetChildCount());

  BookmarkEditorView::BookmarkNode* bb_node = editor_root->GetChild(0);
  // The root should have 2 nodes: folder F1 and F2.
  ASSERT_EQ(2, bb_node->GetChildCount());
  ASSERT_EQ(L"F1", bb_node->GetChild(0)->GetTitle());
  ASSERT_EQ(L"F2", bb_node->GetChild(1)->GetTitle());

  // F1 should have one child, F11
  ASSERT_EQ(1, bb_node->GetChild(0)->GetChildCount());
  ASSERT_EQ(L"F11", bb_node->GetChild(0)->GetChild(0)->GetTitle());

  BookmarkEditorView::BookmarkNode* other_node = editor_root->GetChild(1);
  // Other node should have one child (OF1).
  ASSERT_EQ(1, other_node->GetChildCount());
  ASSERT_EQ(L"OF1", other_node->GetChild(0)->GetTitle());
}

// Changes the title and makes sure parent/visual order doesn't change.
TEST_F(BookmarkEditorViewTest, EditTitleKeepsPosition) {
  BookmarkEditorView editor(profile_.get(), GURL(base_path() + "a"), L"new_a");

  editor.ApplyEdits(editor.tree_model_->GetRoot()->GetChild(0));

  BookmarkBarNode* bb_node =
      profile_->GetBookmarkBarModel()->GetBookmarkBarNode();
  ASSERT_EQ(L"new_a", bb_node->GetChild(0)->GetTitle());
  // The URL shouldn't have changed.
  ASSERT_TRUE(GURL(base_path() + "a") == bb_node->GetChild(0)->GetURL());
}

// Changes the url and makes sure parent/visual order doesn't change.
TEST_F(BookmarkEditorViewTest, EditURLKeepsPosition) {
  BookmarkEditorView editor(profile_.get(), GURL(base_path() + "a"), L"a");

  editor.url_tf_.SetText(UTF8ToWide(GURL(base_path() + "new_a").spec()));

  editor.ApplyEdits(editor.tree_model_->GetRoot()->GetChild(0));

  BookmarkBarNode* bb_node =
      profile_->GetBookmarkBarModel()->GetBookmarkBarNode();
  ASSERT_EQ(L"a", bb_node->GetChild(0)->GetTitle());
  // The URL should have changed.
  ASSERT_TRUE(GURL(base_path() + "new_a") == bb_node->GetChild(0)->GetURL());
}

// Moves 'a' to be a child of the other node.
TEST_F(BookmarkEditorViewTest, ChangeParent) {
  BookmarkEditorView editor(profile_.get(), GURL(base_path() + "a"), L"a");

  editor.ApplyEdits(editor.tree_model_->GetRoot()->GetChild(1));

  BookmarkBarNode* other_node = profile_->GetBookmarkBarModel()->other_node();
  ASSERT_EQ(L"a", other_node->GetChild(2)->GetTitle());
  ASSERT_TRUE(GURL(base_path() + "a") == other_node->GetChild(2)->GetURL());
}

// Changes the URL to a URL that is already starred.
TEST_F(BookmarkEditorViewTest, ChangeURLToExistingURL) {
  BookmarkEditorView editor(profile_.get(), GURL(base_path() + "a"), L"a");

  editor.url_tf_.SetText(UTF8ToWide(GURL(base_path() + "f1a").spec()));

  editor.ApplyEdits(editor.tree_model_->GetRoot()->GetChild(0));

  // Position shouldn't have changed.
  BookmarkBarNode* bb_node =
      profile_->GetBookmarkBarModel()->GetBookmarkBarNode();
  ASSERT_EQ(L"a", bb_node->GetChild(0)->GetTitle());
  // The URL should have changed.
  ASSERT_TRUE(GURL(base_path() + "f1a") == bb_node->GetChild(0)->GetURL());

  // And F1 should have one last child (f1a was removed from it).
  ASSERT_EQ(1, bb_node->GetChild(1)->GetChildCount());
  ASSERT_NE(L"f1a", bb_node->GetChild(1)->GetChild(0)->GetTitle());
}

// Creates a new folder and moves a node to it.
TEST_F(BookmarkEditorViewTest, MoveToNewParent) {
  BookmarkEditorView editor(profile_.get(), GURL(base_path() + "a"), L"a");

  // Create two nodes: "F21" as a child of "F2" and "F211" as a child of "F21".
  BookmarkEditorView::BookmarkNode* f2 =
      editor.tree_model_->GetRoot()->GetChild(0)->GetChild(1);
  BookmarkEditorView::BookmarkNode* f21 = editor.AddNewGroup(f2);
  f21->SetTitle(L"F21");
  BookmarkEditorView::BookmarkNode* f211 = editor.AddNewGroup(f21);
  f211->SetTitle(L"F211");

  // Parent the node to "F21".
  editor.ApplyEdits(f2);

  BookmarkBarNode* bb_node =
      profile_->GetBookmarkBarModel()->GetBookmarkBarNode();
  BookmarkBarNode* mf2 = bb_node->GetChild(1);

  // F2 in the model should have two children now: F21 and the node edited.
  ASSERT_EQ(2, mf2->GetChildCount());
  // F21 should be first.
  ASSERT_EQ(L"F21", mf2->GetChild(0)->GetTitle());
  // Then a.
  ASSERT_EQ(L"a", mf2->GetChild(1)->GetTitle());

  // F21 should have one child, F211.
  BookmarkBarNode* mf21 = mf2->GetChild(0);
  ASSERT_EQ(1, mf21->GetChildCount());
  ASSERT_EQ(L"F211", mf21->GetChild(0)->GetTitle());
}

