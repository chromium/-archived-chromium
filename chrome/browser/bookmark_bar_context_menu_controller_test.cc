// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmark_bar_context_menu_controller.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/views/bookmark_bar_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/browser/page_navigator.h"
#include "chrome/test/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "generated_resources.h"

namespace {

// PageNavigator implementation that records the URL.
class TestingPageNavigator : public PageNavigator {
 public:
  virtual void OpenURL(const GURL& url, const GURL& referrer,
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
    profile_->CreateBookmarkModel(true);

    model_ = profile_->GetBookmarkModel();

    bb_view_.reset(new BookmarkBarView(profile_.get(), NULL));
    bb_view_->SetPageNavigator(&navigator_);

    AddTestData();
  }

  virtual void TearDown() {
    BookmarkBarView::testing_ = false;

    // Flush the message loop to make Purify happy.
    message_loop_.RunAllPending();
  }

 protected:
  MessageLoopForUI message_loop_;
  scoped_ptr<TestingProfile> profile_;
  BookmarkModel* model_;
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
    BookmarkNode* f1 =
        model_->AddGroup(model_->GetBookmarkBarNode(), 1, L"F1");
    model_->AddURL(f1, 0, L"f1a", GURL(test_base + "f1a"));
    BookmarkNode* f11 = model_->AddGroup(f1, 1, L"F11");
    model_->AddURL(f11, 0, L"f11a", GURL(test_base + "f11a"));
    model_->AddGroup(model_->GetBookmarkBarNode(), 2, L"F2");
  }
};

// Tests Deleting from the menu.
TEST_F(BookmarkBarContextMenuControllerTest, DeleteURL) {
  BookmarkBarContextMenuController controller(
      bb_view_.get(), model_->GetBookmarkBarNode()->GetChild(0));
  GURL url = model_->GetBookmarkBarNode()->GetChild(0)->GetURL();
  ASSERT_TRUE(controller.IsCommandEnabled(IDS_BOOKMARK_BAR_REMOVE));
  // Delete the URL.
  controller.ExecuteCommand(IDS_BOOKMARK_BAR_REMOVE);
  // Model shouldn't have URL anymore.
  ASSERT_FALSE(model_->IsBookmarked(url));
}

// Tests open all on a folder with a couple of bookmarks.
TEST_F(BookmarkBarContextMenuControllerTest, OpenAll) {
  BookmarkNode* folder = model_->GetBookmarkBarNode()->GetChild(1);
  BookmarkBarContextMenuController controller(bb_view_.get(), folder);
  ASSERT_TRUE(controller.IsCommandEnabled(IDS_BOOMARK_BAR_OPEN_ALL));
  ASSERT_TRUE(controller.IsCommandEnabled(IDS_BOOMARK_BAR_OPEN_ALL_INCOGNITO));
  ASSERT_TRUE(controller.IsCommandEnabled(IDS_BOOMARK_BAR_OPEN_ALL_NEW_WINDOW));
  // Open it.
  controller.ExecuteCommand(IDS_BOOMARK_BAR_OPEN_ALL);
  // Should have navigated to F1's children.
  ASSERT_EQ(2, navigator_.urls_.size());
  ASSERT_TRUE(folder->GetChild(0)->GetURL() == navigator_.urls_[0]);
  ASSERT_TRUE(folder->GetChild(1)->GetChild(0)->GetURL() ==
              navigator_.urls_[1]);

  // Make sure incognito is disabled when OTR.
  profile_->set_off_the_record(true);
  ASSERT_TRUE(controller.IsCommandEnabled(IDS_BOOMARK_BAR_OPEN_ALL));
  ASSERT_FALSE(controller.IsCommandEnabled(IDS_BOOMARK_BAR_OPEN_ALL_INCOGNITO));
  ASSERT_TRUE(
      controller.IsCommandEnabled(IDS_BOOMARK_BAR_OPEN_ALL_NEW_WINDOW));
}

// Tests that menus are appropriately disabled for empty folders.
TEST_F(BookmarkBarContextMenuControllerTest, DisableForEmptyFolder) {
  BookmarkNode* folder = model_->GetBookmarkBarNode()->GetChild(2);
  BookmarkBarContextMenuController controller(bb_view_.get(), folder);
  EXPECT_FALSE(controller.IsCommandEnabled(IDS_BOOMARK_BAR_OPEN_ALL));
  EXPECT_FALSE(
      controller.IsCommandEnabled(IDS_BOOMARK_BAR_OPEN_ALL_NEW_WINDOW));
}

// Tests the enabled state of open incognito.
TEST_F(BookmarkBarContextMenuControllerTest, DisableIncognito) {
  BookmarkBarContextMenuController controller(
      bb_view_.get(), model_->GetBookmarkBarNode()->GetChild(0));
  EXPECT_TRUE(controller.IsCommandEnabled(IDS_BOOMARK_BAR_OPEN_INCOGNITO));
  profile_->set_off_the_record(true);
  EXPECT_FALSE(controller.IsCommandEnabled(IDS_BOOMARK_BAR_OPEN_INCOGNITO));
}
