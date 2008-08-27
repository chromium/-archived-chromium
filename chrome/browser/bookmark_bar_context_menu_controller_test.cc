// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmark_bar_context_menu_controller.h"
#include "chrome/browser/bookmark_bar_model.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/views/bookmark_bar_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/browser/page_navigator.h"
#include "chrome/test/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// PageNavigator implementation that records the URL.
class TestingPageNavigator : public PageNavigator {
 public:
  virtual void OpenURL(const GURL& url,
                       WindowOpenDisposition disposition,
                       PageTransition::Type transition) {
    urls_.push_back(url);
  }

  std::vector<GURL> urls_;
};

}  // namespace

class BookmarkBarContextMenuControllerTest : public testing::Test {
 public:
  BookmarkBarContextMenuControllerTest()
      : bb_view_(NULL), model_(NULL) {
  }

  virtual void SetUp() {
    BookmarkBarView::testing_ = true;

    profile_.reset(new TestingProfile());
    profile_->set_has_history_service(true);
    profile_->CreateBookmarkBarModel(true);

    model_ = profile_->GetBookmarkBarModel();

    bb_view_.reset(new BookmarkBarView(profile_.get(), NULL));
    bb_view_->SetPageNavigator(&navigator_);

    AddTestData();
  }

  virtual void TearDown() {
    BookmarkBarView::testing_ = false;
  }

 protected:
  MessageLoopForUI message_loop_;
  scoped_ptr<TestingProfile> profile_;
  BookmarkBarModel* model_;
  scoped_ptr<BookmarkBarView> bb_view_;
  TestingPageNavigator navigator_;

 private:
  // Creates the following structure:
  // a
  // F1
  //  f1a
  //  F11
  //   f11a
  // F2
  void AddTestData() {
    std::string test_base = "file:///c:/tmp/";

    model_->AddURL(model_->GetBookmarkBarNode(), 0, L"a",
                   GURL(test_base + "a"));
    BookmarkBarNode* f1 =
        model_->AddGroup(model_->GetBookmarkBarNode(), 1, L"F1");
    model_->AddURL(f1, 0, L"f1a", GURL(test_base + "f1a"));
    BookmarkBarNode* f11 = model_->AddGroup(f1, 1, L"F11");
    model_->AddURL(f11, 0, L"f11a", GURL(test_base + "f11a"));
    model_->AddGroup(model_->GetBookmarkBarNode(), 2, L"F2");
  }
};

// Tests Deleting from the menu.
TEST_F(BookmarkBarContextMenuControllerTest, DeleteURL) {
  BookmarkBarContextMenuController controller(
      bb_view_.get(), model_->GetBookmarkBarNode()->GetChild(0));
  GURL url = model_->GetBookmarkBarNode()->GetChild(0)->GetURL();
  ASSERT_TRUE(controller.IsCommandEnabled(
                  BookmarkBarContextMenuController::delete_bookmark_id));
  // Delete the URL.
  controller.ExecuteCommand(
      BookmarkBarContextMenuController::delete_bookmark_id);
  // Model shouldn't have URL anymore.
  ASSERT_TRUE(model_->GetNodeByURL(url) == NULL);
}

// Tests openning from the menu.
TEST_F(BookmarkBarContextMenuControllerTest, OpenURL) {
  BookmarkBarContextMenuController controller(
      bb_view_.get(), model_->GetBookmarkBarNode()->GetChild(0));
  GURL url = model_->GetBookmarkBarNode()->GetChild(0)->GetURL();
  ASSERT_TRUE(controller.IsCommandEnabled(
                  BookmarkBarContextMenuController::open_bookmark_id));
  // Open it.
  controller.ExecuteCommand(
      BookmarkBarContextMenuController::open_bookmark_id);
  // Should have navigated to it.
  ASSERT_EQ(1, navigator_.urls_.size());
  ASSERT_TRUE(url == navigator_.urls_[0]);
}

// Tests open all on a folder with a couple of bookmarks.
TEST_F(BookmarkBarContextMenuControllerTest, OpenAll) {
  BookmarkBarNode* folder = model_->GetBookmarkBarNode()->GetChild(1);
  BookmarkBarContextMenuController controller(bb_view_.get(), folder);
  ASSERT_TRUE(controller.IsCommandEnabled(
      BookmarkBarContextMenuController::open_all_bookmarks_id));
  ASSERT_TRUE(controller.IsCommandEnabled(
      BookmarkBarContextMenuController::open_all_bookmarks_in_new_window_id));
  // Open it.
  controller.ExecuteCommand(
      BookmarkBarContextMenuController::open_all_bookmarks_id);
  // Should have navigated to F1's children.
  ASSERT_EQ(2, navigator_.urls_.size());
  ASSERT_TRUE(folder->GetChild(0)->GetURL() == navigator_.urls_[0]);
  ASSERT_TRUE(folder->GetChild(1)->GetChild(0)->GetURL() ==
              navigator_.urls_[1]);
}

// Tests that menus are appropriately disabled for empty folders.
TEST_F(BookmarkBarContextMenuControllerTest, DisableForEmptyFolder) {
  BookmarkBarNode* folder = model_->GetBookmarkBarNode()->GetChild(2);
  BookmarkBarContextMenuController controller(bb_view_.get(), folder);
  EXPECT_FALSE(controller.IsCommandEnabled(
      BookmarkBarContextMenuController::open_all_bookmarks_id));
  EXPECT_FALSE(controller.IsCommandEnabled(
      BookmarkBarContextMenuController::open_all_bookmarks_in_new_window_id));
}

