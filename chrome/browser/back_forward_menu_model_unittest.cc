// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/back_forward_menu_model.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "chrome/browser/profile_manager.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/tab_contents_factory.h"
#include "chrome/common/url_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
// TODO(port): port this header.
#include "chrome/browser/tab_contents/tab_contents.h"
#elif defined(OS_POSIX)
#include "chrome/common/temp_scaffolding_stubs.h"
#endif

const TabContentsType kHTTPTabContentsType =
    static_cast<TabContentsType>(TAB_CONTENTS_NUM_TYPES + 1);

// Since you can't just instantiate a TabContents, and some of its methods
// are protected, we subclass TabContents with our own testing dummy which
// knows how to drive the base class' NavigationController as URLs are
// loaded. Constructing one of these automatically constructs a
// NavigationController instance which we wrap our RecentPagesModel around.
class BackFwdMenuModelTestTabContents : public TabContents {
 public:
  BackFwdMenuModelTestTabContents() : TabContents(kHTTPTabContentsType) {
  }

  // We do the same thing as the TabContents one (just commit the navigation)
  // but we *don't* want to reset the title since the test looks for this.
  virtual bool NavigateToPendingEntry(bool reload) {
    controller()->CommitPendingEntry();
    return true;
  }

  void UpdateState(const std::wstring& title) {
    NavigationEntry* entry =
      controller()->GetEntryWithPageID(type(), NULL, GetMaxPageID());
    entry->set_title(WideToUTF16Hack(title));
  }
};

// This constructs our fake TabContents.
class BackFwdMenuModelTestTabContentsFactory : public TabContentsFactory {
 public:
  virtual TabContents* CreateInstance() {
    return new BackFwdMenuModelTestTabContents;
  }

  virtual bool CanHandleURL(const GURL& url) {
    return url.SchemeIs(chrome::kHttpScheme);
  }
};

BackFwdMenuModelTestTabContentsFactory factory;

class BackFwdMenuModelTest : public testing::Test {
 public:

  // Overridden from testing::Test
  virtual void SetUp() {
    TabContents::RegisterFactory(kHTTPTabContentsType, &factory);

    // Name a subdirectory of the temp directory.
    ASSERT_TRUE(PathService::Get(base::DIR_TEMP, &test_dir_));
    test_dir_ = test_dir_.Append(FILE_PATH_LITERAL("BackFwdMenuModelTest"));

    // Create a fresh, empty copy of this directory.
    file_util::Delete(test_dir_, true);
    file_util::CreateDirectory(test_dir_);

    profile_path_ = test_dir_;
    profile_path_ = profile_path_.Append(FILE_PATH_LITERAL("New Profile"));

    profile_ = ProfileManager::CreateProfile(
        profile_path_, L"New Profile", L"new-profile", L"");
    ASSERT_TRUE(profile_);
    pm_.AddProfile(profile_);
  }

  virtual void TearDown() {
    TabContents::RegisterFactory(kHTTPTabContentsType, NULL);

    // Removes a profile from the set of currently-loaded profiles.
    pm_.RemoveProfileByPath(profile_path_);

    // Clean up test directory
    ASSERT_TRUE(file_util::Delete(test_dir_, true));
    ASSERT_FALSE(file_util::PathExists(test_dir_));
  }

 protected:
  TabContents* CreateTabContents() {
    TabContents* contents = new BackFwdMenuModelTestTabContents;
    contents->CreateView();
    contents->SetupController(profile_);
    return contents;
  }

  // Forwards a URL "load" request through to our dummy TabContents
  // implementation.
  void LoadURLAndUpdateState(TabContents* contents,
                             const std::string& url,
                             const std::wstring& title) {
    contents->controller()->LoadURL(GURL(url), GURL(), PageTransition::LINK);
    BackFwdMenuModelTestTabContents* rsmttc =
      static_cast<BackFwdMenuModelTestTabContents*>(contents);
    rsmttc->UpdateState(title);
  }

 private:
  MessageLoopForUI message_loop_;
  FilePath test_dir_;
  FilePath profile_path_;
  ProfileManager pm_;
  Profile* profile_;
};

TEST_F(BackFwdMenuModelTest, BasicCase) {
  TabContents* contents = CreateTabContents();

  {
    scoped_ptr<BackForwardMenuModel> back_model(BackForwardMenuModel::Create(
      NULL, BackForwardMenuModel::BACKWARD_MENU_DELEGATE));
    back_model->set_test_tab_contents(contents);

    scoped_ptr<BackForwardMenuModel> forward_model(BackForwardMenuModel::Create(
      NULL, BackForwardMenuModel::FORWARD_MENU_DELEGATE));
    forward_model->set_test_tab_contents(contents);

    EXPECT_EQ(0, back_model->GetTotalItemCount());
    EXPECT_EQ(0, forward_model->GetTotalItemCount());
    EXPECT_FALSE(back_model->ItemHasCommand(1));

    // Seed the controller with a few URLs
    LoadURLAndUpdateState(contents, "http://www.a.com/1", L"A1");
    LoadURLAndUpdateState(contents, "http://www.a.com/2", L"A2");
    LoadURLAndUpdateState(contents, "http://www.a.com/3", L"A3");
    LoadURLAndUpdateState(contents, "http://www.b.com/1", L"B1");
    LoadURLAndUpdateState(contents, "http://www.b.com/2", L"B2");
    LoadURLAndUpdateState(contents, "http://www.c.com/1", L"C1");
    LoadURLAndUpdateState(contents, "http://www.c.com/2", L"C2");
    LoadURLAndUpdateState(contents, "http://www.c.com/3", L"C3");

    // There're two more items here: a separator and a "Show Full History".
    EXPECT_EQ(9, back_model->GetTotalItemCount());
    EXPECT_EQ(0, forward_model->GetTotalItemCount());
    EXPECT_EQ(L"C2", back_model->GetItemLabel(1));
    EXPECT_EQ(L"A1", back_model->GetItemLabel(7));
    EXPECT_EQ(back_model->GetShowFullHistoryLabel(),
              back_model->GetItemLabel(9));

    EXPECT_TRUE(back_model->ItemHasCommand(1));
    EXPECT_TRUE(back_model->ItemHasCommand(7));
    EXPECT_TRUE(back_model->IsSeparator(8));
    EXPECT_TRUE(back_model->ItemHasCommand(9));
    EXPECT_FALSE(back_model->ItemHasCommand(8));
    EXPECT_FALSE(back_model->ItemHasCommand(10));

    contents->controller()->GoToOffset(-7);

    EXPECT_EQ(0, back_model->GetTotalItemCount());
    EXPECT_EQ(9, forward_model->GetTotalItemCount());
    EXPECT_EQ(L"A2", forward_model->GetItemLabel(1));
    EXPECT_EQ(L"C3", forward_model->GetItemLabel(7));
    EXPECT_EQ(forward_model->GetShowFullHistoryLabel(),
              forward_model->GetItemLabel(9));

    EXPECT_TRUE(forward_model->ItemHasCommand(1));
    EXPECT_TRUE(forward_model->ItemHasCommand(7));
    EXPECT_TRUE(forward_model->IsSeparator(8));
    EXPECT_TRUE(forward_model->ItemHasCommand(9));
    EXPECT_FALSE(forward_model->ItemHasCommand(8));
    EXPECT_FALSE(forward_model->ItemHasCommand(10));
    contents->controller()->GoToOffset(4);

    EXPECT_EQ(6, back_model->GetTotalItemCount());
    EXPECT_EQ(5, forward_model->GetTotalItemCount());
    EXPECT_EQ(L"B1", back_model->GetItemLabel(1));
    EXPECT_EQ(L"A1", back_model->GetItemLabel(4));
    EXPECT_EQ(back_model->GetShowFullHistoryLabel(),
              back_model->GetItemLabel(6));
    EXPECT_EQ(L"C1", forward_model->GetItemLabel(1));
    EXPECT_EQ(L"C3", forward_model->GetItemLabel(3));
    EXPECT_EQ(forward_model->GetShowFullHistoryLabel(),
              forward_model->GetItemLabel(5));
  }
  contents->CloseContents();
}

TEST_F(BackFwdMenuModelTest, MaxItemsTest) {
  TabContents* contents = CreateTabContents();

  {
    scoped_ptr<BackForwardMenuModel> back_model(BackForwardMenuModel::Create(
      NULL, BackForwardMenuModel::BACKWARD_MENU_DELEGATE));
    back_model->set_test_tab_contents(contents);

    scoped_ptr<BackForwardMenuModel> forward_model(BackForwardMenuModel::Create(
      NULL, BackForwardMenuModel::FORWARD_MENU_DELEGATE));
    forward_model->set_test_tab_contents(contents);

    // Seed the controller with 32 URLs
    LoadURLAndUpdateState(contents, "http://www.a.com/1", L"A1");
    LoadURLAndUpdateState(contents, "http://www.a.com/2", L"A2");
    LoadURLAndUpdateState(contents, "http://www.a.com/3", L"A3");
    LoadURLAndUpdateState(contents, "http://www.b.com/1", L"B1");
    LoadURLAndUpdateState(contents, "http://www.b.com/2", L"B2");
    LoadURLAndUpdateState(contents, "http://www.b.com/3", L"B3");
    LoadURLAndUpdateState(contents, "http://www.c.com/1", L"C1");
    LoadURLAndUpdateState(contents, "http://www.c.com/2", L"C2");
    LoadURLAndUpdateState(contents, "http://www.c.com/3", L"C3");
    LoadURLAndUpdateState(contents, "http://www.d.com/1", L"D1");
    LoadURLAndUpdateState(contents, "http://www.d.com/2", L"D2");
    LoadURLAndUpdateState(contents, "http://www.d.com/3", L"D3");
    LoadURLAndUpdateState(contents, "http://www.e.com/1", L"E1");
    LoadURLAndUpdateState(contents, "http://www.e.com/2", L"E2");
    LoadURLAndUpdateState(contents, "http://www.e.com/3", L"E3");
    LoadURLAndUpdateState(contents, "http://www.f.com/1", L"F1");
    LoadURLAndUpdateState(contents, "http://www.f.com/2", L"F2");
    LoadURLAndUpdateState(contents, "http://www.f.com/3", L"F3");
    LoadURLAndUpdateState(contents, "http://www.g.com/1", L"G1");
    LoadURLAndUpdateState(contents, "http://www.g.com/2", L"G2");
    LoadURLAndUpdateState(contents, "http://www.g.com/3", L"G3");
    LoadURLAndUpdateState(contents, "http://www.h.com/1", L"H1");
    LoadURLAndUpdateState(contents, "http://www.h.com/2", L"H2");
    LoadURLAndUpdateState(contents, "http://www.h.com/3", L"H3");
    LoadURLAndUpdateState(contents, "http://www.i.com/1", L"I1");
    LoadURLAndUpdateState(contents, "http://www.i.com/2", L"I2");
    LoadURLAndUpdateState(contents, "http://www.i.com/3", L"I3");
    LoadURLAndUpdateState(contents, "http://www.j.com/1", L"J1");
    LoadURLAndUpdateState(contents, "http://www.j.com/2", L"J2");
    LoadURLAndUpdateState(contents, "http://www.j.com/3", L"J3");
    LoadURLAndUpdateState(contents, "http://www.k.com/1", L"K1");
    LoadURLAndUpdateState(contents, "http://www.k.com/2", L"K2");

    // Also there're two more for a separator and a "Show Full History".
    int chapter_stop_offset = 6;
    EXPECT_EQ(BackForwardMenuModel::kMaxHistoryItems + 2 + chapter_stop_offset,
              back_model->GetTotalItemCount());
    EXPECT_EQ(0, forward_model->GetTotalItemCount());
    EXPECT_EQ(L"K1", back_model->GetItemLabel(1));
    EXPECT_EQ(back_model->GetShowFullHistoryLabel(),
      back_model->GetItemLabel(BackForwardMenuModel::kMaxHistoryItems + 2 +
                          chapter_stop_offset));

    // Test for out of bounds (beyond Show Full History).
    EXPECT_FALSE(back_model->ItemHasCommand(
        BackForwardMenuModel::kMaxHistoryItems + chapter_stop_offset + 3));

    EXPECT_TRUE(back_model->ItemHasCommand(
        BackForwardMenuModel::kMaxHistoryItems));
    EXPECT_TRUE(back_model->IsSeparator(
      BackForwardMenuModel::kMaxHistoryItems + 1));

    contents->controller()->GoToIndex(0);

    EXPECT_EQ(BackForwardMenuModel::kMaxHistoryItems + 2 + chapter_stop_offset,
              forward_model->GetTotalItemCount());
    EXPECT_EQ(0, back_model->GetTotalItemCount());
    EXPECT_EQ(L"A2", forward_model->GetItemLabel(1));
    EXPECT_EQ(forward_model->GetShowFullHistoryLabel(),
      forward_model->GetItemLabel(BackForwardMenuModel::kMaxHistoryItems + 2 +
                             chapter_stop_offset));

    // Out of bounds
    EXPECT_FALSE(forward_model->ItemHasCommand(
        BackForwardMenuModel::kMaxHistoryItems + 3 + chapter_stop_offset));

    EXPECT_TRUE(forward_model->ItemHasCommand(
        BackForwardMenuModel::kMaxHistoryItems));
    EXPECT_TRUE(forward_model->IsSeparator(
        BackForwardMenuModel::kMaxHistoryItems + 1));
  }
  contents->CloseContents();
}

void ValidateModel(BackForwardMenuModel* model, int history_items,
                   int chapter_stops) {
  int h = std::min(BackForwardMenuModel::kMaxHistoryItems, history_items);
  int c = std::min(BackForwardMenuModel::kMaxChapterStops, chapter_stops);
  EXPECT_EQ(h, model->GetHistoryItemCount());
  EXPECT_EQ(c, model->GetChapterStopCount(h));
  if (h > 0)
    h += 2;  // separator and View History link
  if (c > 0)
    ++c;
  EXPECT_EQ(h + c, model->GetTotalItemCount());
}

TEST_F(BackFwdMenuModelTest, ChapterStops) {
  TabContents* contents = CreateTabContents();

  {
    scoped_ptr<BackForwardMenuModel> back_model(BackForwardMenuModel::Create(
      NULL, BackForwardMenuModel::BACKWARD_MENU_DELEGATE));
    back_model->set_test_tab_contents(contents);

    scoped_ptr<BackForwardMenuModel> forward_model(BackForwardMenuModel::Create(
      NULL, BackForwardMenuModel::FORWARD_MENU_DELEGATE));
    forward_model->set_test_tab_contents(contents);

    // Seed the controller with 32 URLs.
    int i = 0;
    LoadURLAndUpdateState(contents, "http://www.a.com/1", L"A1"); // 0
    ValidateModel(back_model.get(), i++, 0);
    LoadURLAndUpdateState(contents, "http://www.a.com/2", L"A2");
    ValidateModel(back_model.get(), i++, 0);
    LoadURLAndUpdateState(contents, "http://www.a.com/3", L"A3");
    ValidateModel(back_model.get(), i++, 0);
    LoadURLAndUpdateState(contents, "http://www.b.com/1", L"B1");
    ValidateModel(back_model.get(), i++, 0);
    LoadURLAndUpdateState(contents, "http://www.b.com/2", L"B2");
    ValidateModel(back_model.get(), i++, 0);
    LoadURLAndUpdateState(contents, "http://www.b.com/3", L"B3"); // 5
    ValidateModel(back_model.get(), i++, 0);
    LoadURLAndUpdateState(contents, "http://www.c.com/1", L"C1");
    ValidateModel(back_model.get(), i++, 0);
    LoadURLAndUpdateState(contents, "http://www.c.com/2", L"C2");
    ValidateModel(back_model.get(), i++, 0);
    LoadURLAndUpdateState(contents, "http://www.c.com/3", L"C3");
    ValidateModel(back_model.get(), i++, 0);
    LoadURLAndUpdateState(contents, "http://www.d.com/1", L"D1");
    ValidateModel(back_model.get(), i++, 0);
    LoadURLAndUpdateState(contents, "http://www.d.com/2", L"D2"); // 10
    ValidateModel(back_model.get(), i++, 0);
    LoadURLAndUpdateState(contents, "http://www.d.com/3", L"D3");
    ValidateModel(back_model.get(), i++, 0);
    LoadURLAndUpdateState(contents, "http://www.e.com/1", L"E1");
    ValidateModel(back_model.get(), i++, 0);
    LoadURLAndUpdateState(contents, "http://www.e.com/2", L"E2");
    ValidateModel(back_model.get(), i++, 0);
    LoadURLAndUpdateState(contents, "http://www.e.com/3", L"E3");
    ValidateModel(back_model.get(), i++, 0);
    LoadURLAndUpdateState(contents, "http://www.f.com/1", L"F1"); // 15
    ValidateModel(back_model.get(), i++, 1);
    LoadURLAndUpdateState(contents, "http://www.f.com/2", L"F2");
    ValidateModel(back_model.get(), i++, 1);
    LoadURLAndUpdateState(contents, "http://www.f.com/3", L"F3");
    ValidateModel(back_model.get(), i++, 1);
    LoadURLAndUpdateState(contents, "http://www.g.com/1", L"G1");
    ValidateModel(back_model.get(), i++, 2);
    LoadURLAndUpdateState(contents, "http://www.g.com/2", L"G2");
    ValidateModel(back_model.get(), i++, 2);
    LoadURLAndUpdateState(contents, "http://www.g.com/3", L"G3"); // 20
    ValidateModel(back_model.get(), i++, 2);
    LoadURLAndUpdateState(contents, "http://www.h.com/1", L"H1");
    ValidateModel(back_model.get(), i++, 3);
    LoadURLAndUpdateState(contents, "http://www.h.com/2", L"H2");
    ValidateModel(back_model.get(), i++, 3);
    LoadURLAndUpdateState(contents, "http://www.h.com/3", L"H3");
    ValidateModel(back_model.get(), i++, 3);
    LoadURLAndUpdateState(contents, "http://www.i.com/1", L"I1");
    ValidateModel(back_model.get(), i++, 4);
    LoadURLAndUpdateState(contents, "http://www.i.com/2", L"I2"); // 25
    ValidateModel(back_model.get(), i++, 4);
    LoadURLAndUpdateState(contents, "http://www.i.com/3", L"I3");
    ValidateModel(back_model.get(), i++, 4);
    LoadURLAndUpdateState(contents, "http://www.j.com/1", L"J1");
    ValidateModel(back_model.get(), i++, 5);
    LoadURLAndUpdateState(contents, "http://www.j.com/2", L"J2");
    ValidateModel(back_model.get(), i++, 5);
    LoadURLAndUpdateState(contents, "http://www.j.com/3", L"J3");
    ValidateModel(back_model.get(), i++, 5);
    LoadURLAndUpdateState(contents, "http://www.k.com/1", L"K1"); // 30
    ValidateModel(back_model.get(), i++, 6);
    LoadURLAndUpdateState(contents, "http://www.k.com/2", L"K2");
    ValidateModel(back_model.get(), i++, 6);
    LoadURLAndUpdateState(contents, "http://www.k.com/3", L"K3"); // 32
    ValidateModel(back_model.get(), i++, 6);

    // Check to see if the chapter stops have the right labels.
    int index = BackForwardMenuModel::kMaxHistoryItems + 1;
    EXPECT_EQ(L"", back_model->GetItemLabel(index++));  // separator.
    EXPECT_EQ(L"F3", back_model->GetItemLabel(index++));
    EXPECT_EQ(L"E3", back_model->GetItemLabel(index++));
    EXPECT_EQ(L"D3", back_model->GetItemLabel(index++));
    EXPECT_EQ(L"C3", back_model->GetItemLabel(index++));
    EXPECT_EQ(L"B3", back_model->GetItemLabel(index));  // max 5 chapter stops.
    EXPECT_EQ(L"", back_model->GetItemLabel(index + 1));  // separator.
    EXPECT_EQ(back_model->GetShowFullHistoryLabel(),
              back_model->GetItemLabel(index + 2));

    // If we go back two we should still see the same chapter stop at the end.
    contents->controller()->GoBack();
    EXPECT_EQ(L"B3", back_model->GetItemLabel(index));
    contents->controller()->GoBack();
    EXPECT_EQ(L"B3", back_model->GetItemLabel(index));
    // But if we go back again, it should change.
    contents->controller()->GoBack();
    EXPECT_EQ(L"A3", back_model->GetItemLabel(index));
    contents->controller()->GoBack();
    EXPECT_EQ(L"A3", back_model->GetItemLabel(index));
    contents->controller()->GoBack();
    EXPECT_EQ(L"A3", back_model->GetItemLabel(index));
    contents->controller()->GoBack();
    EXPECT_EQ(L"", back_model->GetItemLabel(index));  // is now a separator.
    contents->controller()->GoToOffset(6);  // undo our position change.

    // Go back enough to make sure no chapter stops should appear.
    contents->controller()->GoToOffset(-BackForwardMenuModel::kMaxHistoryItems);
    ValidateModel(forward_model.get(), BackForwardMenuModel::kMaxHistoryItems,
                  0);
    // Go forward (still no chapter stop)
    contents->controller()->GoForward();
    ValidateModel(forward_model.get(),
                  BackForwardMenuModel::kMaxHistoryItems - 1, 0);
    // Go back two (one chapter stop should show up)
    contents->controller()->GoBack();
    contents->controller()->GoBack();
    ValidateModel(forward_model.get(),
                  BackForwardMenuModel::kMaxHistoryItems, 1);

    // Go to beginning.
    contents->controller()->GoToIndex(0);

    // Check to see if the chapter stops have the right labels.
    index = BackForwardMenuModel::kMaxHistoryItems + 1;
    EXPECT_EQ(L"", forward_model->GetItemLabel(index++));    // separator.
    EXPECT_EQ(L"E3", forward_model->GetItemLabel(index++));
    EXPECT_EQ(L"F3", forward_model->GetItemLabel(index++));
    EXPECT_EQ(L"G3", forward_model->GetItemLabel(index++));
    EXPECT_EQ(L"H3", forward_model->GetItemLabel(index++));
    // max 5 chapter stops.
    EXPECT_EQ(L"I3", forward_model->GetItemLabel(index));
    EXPECT_EQ(L"", forward_model->GetItemLabel(index + 1));  // separator.
    EXPECT_EQ(forward_model->GetShowFullHistoryLabel(),
      forward_model->GetItemLabel(index + 2));

    // If we advance one we should still see the same chapter stop at the end.
    contents->controller()->GoForward();
    EXPECT_EQ(L"I3", forward_model->GetItemLabel(index));
    // But if we advance one again, it should change.
    contents->controller()->GoForward();
    EXPECT_EQ(L"J3", forward_model->GetItemLabel(index));
    contents->controller()->GoForward();
    EXPECT_EQ(L"J3", forward_model->GetItemLabel(index));
    contents->controller()->GoForward();
    EXPECT_EQ(L"J3", forward_model->GetItemLabel(index));
    contents->controller()->GoForward();
    EXPECT_EQ(L"K3", forward_model->GetItemLabel(index));

    // Now test the boundary cases by using the chapter stop function directly.
    // Out of bounds, first too far right (incrementing), then too far left.
    EXPECT_EQ(-1, back_model->GetIndexOfNextChapterStop(33, false));
    EXPECT_EQ(-1, back_model->GetIndexOfNextChapterStop(-1, true));
    // Test being at end and going right, then at beginning going left.
    EXPECT_EQ(-1, back_model->GetIndexOfNextChapterStop(32, true));
    EXPECT_EQ(-1, back_model->GetIndexOfNextChapterStop(0, false));
    // Test success: beginning going right and end going left.
    EXPECT_EQ(2,  back_model->GetIndexOfNextChapterStop(0, true));
    EXPECT_EQ(29, back_model->GetIndexOfNextChapterStop(32, false));
    // Now see when the chapter stops begin to show up.
    EXPECT_EQ(-1, back_model->GetIndexOfNextChapterStop(1, false));
    EXPECT_EQ(-1, back_model->GetIndexOfNextChapterStop(2, false));
    EXPECT_EQ(2,  back_model->GetIndexOfNextChapterStop(3, false));
    // Now see when the chapter stops end.
    EXPECT_EQ(32, back_model->GetIndexOfNextChapterStop(30, true));
    EXPECT_EQ(32, back_model->GetIndexOfNextChapterStop(31, true));
    EXPECT_EQ(-1, back_model->GetIndexOfNextChapterStop(32, true));

    // Bug found during review (two different sites, but first wasn't considered
    // a chapter-stop).
    contents->controller()->GoToIndex(0);  // Go to A1.
    LoadURLAndUpdateState(contents, "http://www.b.com/1", L"B1");
    EXPECT_EQ(0, back_model->GetIndexOfNextChapterStop(1, false));
    EXPECT_EQ(1, back_model->GetIndexOfNextChapterStop(0, true));

    // Now see if it counts 'www.x.com' and 'mail.x.com' as same domain, which
    // it should.
    contents->controller()->GoToIndex(0);  // Go to A1.
    LoadURLAndUpdateState(contents, "http://mail.a.com/2", L"A2-mail");
    LoadURLAndUpdateState(contents, "http://www.b.com/1", L"B1");
    LoadURLAndUpdateState(contents, "http://mail.b.com/2", L"B2-mail");
    LoadURLAndUpdateState(contents, "http://new.site.com", L"new");
    EXPECT_EQ(1, back_model->GetIndexOfNextChapterStop(0, true));
    EXPECT_EQ(3, back_model->GetIndexOfNextChapterStop(1, true));
    EXPECT_EQ(3, back_model->GetIndexOfNextChapterStop(2, true));
    EXPECT_EQ(4, back_model->GetIndexOfNextChapterStop(3, true));
    // And try backwards as well.
    EXPECT_EQ(3, back_model->GetIndexOfNextChapterStop(4, false));
    EXPECT_EQ(1, back_model->GetIndexOfNextChapterStop(3, false));
    EXPECT_EQ(1, back_model->GetIndexOfNextChapterStop(2, false));
    EXPECT_EQ(-1, back_model->GetIndexOfNextChapterStop(1, false));
  }
  contents->CloseContents();
}
