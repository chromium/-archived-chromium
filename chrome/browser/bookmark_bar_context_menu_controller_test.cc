// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

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
  BookmarkBarContextMenuControllerTest() : bb_view_(NULL), model_(NULL) {
  }

  virtual void SetUp() {
    BookmarkBarView::testing_ = true;

    profile_.reset(new TestingProfile());
    profile_->set_has_history_service(true);
    profile_->CreateBookmarkBarModel();

    model_ = profile_->GetBookmarkBarModel();

    bb_view_ = new BookmarkBarView(profile_.get(), NULL);
    bb_view_->SetPageNavigator(&navigator_);

    AddTestData();
  }

  virtual void TearDown() {
    BookmarkBarView::testing_ = false;
  }

 protected:
  BookmarkBarModel* model_;
  BookmarkBarView* bb_view_;
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

  scoped_ptr<TestingProfile> profile_;
};

// Tests Deleting from the menu.
TEST_F(BookmarkBarContextMenuControllerTest, DeleteURL) {
  BookmarkBarContextMenuController controller(
      bb_view_, model_->GetBookmarkBarNode()->GetChild(0));
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
      bb_view_, model_->GetBookmarkBarNode()->GetChild(0));
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
  BookmarkBarContextMenuController controller(bb_view_, folder);
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
  BookmarkBarContextMenuController controller(bb_view_, folder);
  EXPECT_FALSE(controller.IsCommandEnabled(
      BookmarkBarContextMenuController::open_all_bookmarks_id));
  EXPECT_FALSE(controller.IsCommandEnabled(
      BookmarkBarContextMenuController::open_all_bookmarks_in_new_window_id));
}
