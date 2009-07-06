// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/back_forward_menu_model.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "chrome/browser/profile_manager.h"
#include "chrome/browser/renderer_host/test/test_render_view_host.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/url_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

class BackFwdMenuModelTest : public RenderViewHostTestHarness {
 public:
   void ValidateModel(BackForwardMenuModel* model, int history_items,
                      int chapter_stops) {
    int h = std::min(BackForwardMenuModel::kMaxHistoryItems, history_items);
    int c = std::min(BackForwardMenuModel::kMaxChapterStops, chapter_stops);
    EXPECT_EQ(h, model->GetHistoryItemCount());
    EXPECT_EQ(c, model->GetChapterStopCount(h));
    if (h > 0)
      h += 2;  // Separator and View History link.
    if (c > 0)
      ++c;
    EXPECT_EQ(h + c, model->GetTotalItemCount());
  }

  void LoadURLAndUpdateState(const char* url, const char* title) {
    NavigateAndCommit(GURL(url));
    controller().GetLastCommittedEntry()->set_title(UTF8ToUTF16(title));
  }

  // Navigate back or forward the given amount and commits the entry (which
  // will be pending after we ask to navigate there).
  void NavigateToOffset(int offset) {
    controller().GoToOffset(offset);
    const NavigationEntry* entry = controller().pending_entry();
    rvh()->SendNavigate(entry->page_id(), entry->url());
  }

  // Same as NavigateToOffset but goes to an absolute index.
  void NavigateToIndex(int index) {
    controller().GoToIndex(index);
    const NavigationEntry* entry = controller().pending_entry();
    rvh()->SendNavigate(entry->page_id(), entry->url());
  }

  // Goes back/forward and commits the load.
  void GoBack() {
    controller().GoBack();
    const NavigationEntry* entry = controller().pending_entry();
    rvh()->SendNavigate(entry->page_id(), entry->url());
  }
  void GoForward() {
    controller().GoForward();
    const NavigationEntry* entry = controller().pending_entry();
    rvh()->SendNavigate(entry->page_id(), entry->url());
  }
};

TEST_F(BackFwdMenuModelTest, BasicCase) {
  scoped_ptr<BackForwardMenuModel> back_model(new BackForwardMenuModel(
      NULL, BackForwardMenuModel::BACKWARD_MENU));
  back_model->set_test_tab_contents(contents());

  scoped_ptr<BackForwardMenuModel> forward_model(new BackForwardMenuModel(
      NULL, BackForwardMenuModel::FORWARD_MENU));
  forward_model->set_test_tab_contents(contents());

  EXPECT_EQ(0, back_model->GetTotalItemCount());
  EXPECT_EQ(0, forward_model->GetTotalItemCount());
  EXPECT_FALSE(back_model->ItemHasCommand(1));

  // Seed the controller with a few URLs
  LoadURLAndUpdateState("http://www.a.com/1", "A1");
  LoadURLAndUpdateState("http://www.a.com/2", "A2");
  LoadURLAndUpdateState("http://www.a.com/3", "A3");
  LoadURLAndUpdateState("http://www.b.com/1", "B1");
  LoadURLAndUpdateState("http://www.b.com/2", "B2");
  LoadURLAndUpdateState("http://www.c.com/1", "C1");
  LoadURLAndUpdateState("http://www.c.com/2", "C2");
  LoadURLAndUpdateState("http://www.c.com/3", "C3");

  // There're two more items here: a separator and a "Show Full History".
  EXPECT_EQ(9, back_model->GetTotalItemCount());
  EXPECT_EQ(0, forward_model->GetTotalItemCount());
  EXPECT_EQ(ASCIIToUTF16("C2"), back_model->GetItemLabel(1));
  EXPECT_EQ(ASCIIToUTF16("A1"), back_model->GetItemLabel(7));
  EXPECT_EQ(back_model->GetShowFullHistoryLabel(),
            back_model->GetItemLabel(9));

  EXPECT_TRUE(back_model->ItemHasCommand(1));
  EXPECT_TRUE(back_model->ItemHasCommand(7));
  EXPECT_TRUE(back_model->IsSeparator(8));
  EXPECT_TRUE(back_model->ItemHasCommand(9));
  EXPECT_FALSE(back_model->ItemHasCommand(8));
  EXPECT_FALSE(back_model->ItemHasCommand(10));

  NavigateToOffset(-7);

  EXPECT_EQ(0, back_model->GetTotalItemCount());
  EXPECT_EQ(9, forward_model->GetTotalItemCount());
  EXPECT_EQ(ASCIIToUTF16("A2"), forward_model->GetItemLabel(1));
  EXPECT_EQ(ASCIIToUTF16("C3"), forward_model->GetItemLabel(7));
  EXPECT_EQ(forward_model->GetShowFullHistoryLabel(),
            forward_model->GetItemLabel(9));

  EXPECT_TRUE(forward_model->ItemHasCommand(1));
  EXPECT_TRUE(forward_model->ItemHasCommand(7));
  EXPECT_TRUE(forward_model->IsSeparator(8));
  EXPECT_TRUE(forward_model->ItemHasCommand(9));
  EXPECT_FALSE(forward_model->ItemHasCommand(8));
  EXPECT_FALSE(forward_model->ItemHasCommand(10));
  NavigateToOffset(4);

  EXPECT_EQ(6, back_model->GetTotalItemCount());
  EXPECT_EQ(5, forward_model->GetTotalItemCount());
  EXPECT_EQ(ASCIIToUTF16("B1"), back_model->GetItemLabel(1));
  EXPECT_EQ(ASCIIToUTF16("A1"), back_model->GetItemLabel(4));
  EXPECT_EQ(back_model->GetShowFullHistoryLabel(),
            back_model->GetItemLabel(6));
  EXPECT_EQ(ASCIIToUTF16("C1"), forward_model->GetItemLabel(1));
  EXPECT_EQ(ASCIIToUTF16("C3"), forward_model->GetItemLabel(3));
  EXPECT_EQ(forward_model->GetShowFullHistoryLabel(),
            forward_model->GetItemLabel(5));
}

TEST_F(BackFwdMenuModelTest, MaxItemsTest) {
  scoped_ptr<BackForwardMenuModel> back_model(new BackForwardMenuModel(
      NULL, BackForwardMenuModel::BACKWARD_MENU));
  back_model->set_test_tab_contents(contents());

  scoped_ptr<BackForwardMenuModel> forward_model(new BackForwardMenuModel(
      NULL, BackForwardMenuModel::FORWARD_MENU));
  forward_model->set_test_tab_contents(contents());

  // Seed the controller with 32 URLs
  LoadURLAndUpdateState("http://www.a.com/1", "A1");
  LoadURLAndUpdateState("http://www.a.com/2", "A2");
  LoadURLAndUpdateState("http://www.a.com/3", "A3");
  LoadURLAndUpdateState("http://www.b.com/1", "B1");
  LoadURLAndUpdateState("http://www.b.com/2", "B2");
  LoadURLAndUpdateState("http://www.b.com/3", "B3");
  LoadURLAndUpdateState("http://www.c.com/1", "C1");
  LoadURLAndUpdateState("http://www.c.com/2", "C2");
  LoadURLAndUpdateState("http://www.c.com/3", "C3");
  LoadURLAndUpdateState("http://www.d.com/1", "D1");
  LoadURLAndUpdateState("http://www.d.com/2", "D2");
  LoadURLAndUpdateState("http://www.d.com/3", "D3");
  LoadURLAndUpdateState("http://www.e.com/1", "E1");
  LoadURLAndUpdateState("http://www.e.com/2", "E2");
  LoadURLAndUpdateState("http://www.e.com/3", "E3");
  LoadURLAndUpdateState("http://www.f.com/1", "F1");
  LoadURLAndUpdateState("http://www.f.com/2", "F2");
  LoadURLAndUpdateState("http://www.f.com/3", "F3");
  LoadURLAndUpdateState("http://www.g.com/1", "G1");
  LoadURLAndUpdateState("http://www.g.com/2", "G2");
  LoadURLAndUpdateState("http://www.g.com/3", "G3");
  LoadURLAndUpdateState("http://www.h.com/1", "H1");
  LoadURLAndUpdateState("http://www.h.com/2", "H2");
  LoadURLAndUpdateState("http://www.h.com/3", "H3");
  LoadURLAndUpdateState("http://www.i.com/1", "I1");
  LoadURLAndUpdateState("http://www.i.com/2", "I2");
  LoadURLAndUpdateState("http://www.i.com/3", "I3");
  LoadURLAndUpdateState("http://www.j.com/1", "J1");
  LoadURLAndUpdateState("http://www.j.com/2", "J2");
  LoadURLAndUpdateState("http://www.j.com/3", "J3");
  LoadURLAndUpdateState("http://www.k.com/1", "K1");
  LoadURLAndUpdateState("http://www.k.com/2", "K2");

  // Also there're two more for a separator and a "Show Full History".
  int chapter_stop_offset = 6;
  EXPECT_EQ(BackForwardMenuModel::kMaxHistoryItems + 2 + chapter_stop_offset,
            back_model->GetTotalItemCount());
  EXPECT_EQ(0, forward_model->GetTotalItemCount());
  EXPECT_EQ(ASCIIToUTF16("K1"), back_model->GetItemLabel(1));
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

  NavigateToIndex(0);

  EXPECT_EQ(BackForwardMenuModel::kMaxHistoryItems + 2 + chapter_stop_offset,
            forward_model->GetTotalItemCount());
  EXPECT_EQ(0, back_model->GetTotalItemCount());
  EXPECT_EQ(ASCIIToUTF16("A2"), forward_model->GetItemLabel(1));
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

TEST_F(BackFwdMenuModelTest, ChapterStops) {
  scoped_ptr<BackForwardMenuModel> back_model(new BackForwardMenuModel(
    NULL, BackForwardMenuModel::BACKWARD_MENU));
  back_model->set_test_tab_contents(contents());

  scoped_ptr<BackForwardMenuModel> forward_model(new BackForwardMenuModel(
      NULL, BackForwardMenuModel::FORWARD_MENU));
  forward_model->set_test_tab_contents(contents());

  // Seed the controller with 32 URLs.
  int i = 0;
  LoadURLAndUpdateState("http://www.a.com/1", "A1"); // 0
  ValidateModel(back_model.get(), i++, 0);
  LoadURLAndUpdateState("http://www.a.com/2", "A2");
  ValidateModel(back_model.get(), i++, 0);
  LoadURLAndUpdateState("http://www.a.com/3", "A3");
  ValidateModel(back_model.get(), i++, 0);
  LoadURLAndUpdateState("http://www.b.com/1", "B1");
  ValidateModel(back_model.get(), i++, 0);
  LoadURLAndUpdateState("http://www.b.com/2", "B2");
  ValidateModel(back_model.get(), i++, 0);
  LoadURLAndUpdateState("http://www.b.com/3", "B3"); // 5
  ValidateModel(back_model.get(), i++, 0);
  LoadURLAndUpdateState("http://www.c.com/1", "C1");
  ValidateModel(back_model.get(), i++, 0);
  LoadURLAndUpdateState("http://www.c.com/2", "C2");
  ValidateModel(back_model.get(), i++, 0);
  LoadURLAndUpdateState("http://www.c.com/3", "C3");
  ValidateModel(back_model.get(), i++, 0);
  LoadURLAndUpdateState("http://www.d.com/1", "D1");
  ValidateModel(back_model.get(), i++, 0);
  LoadURLAndUpdateState("http://www.d.com/2", "D2"); // 10
  ValidateModel(back_model.get(), i++, 0);
  LoadURLAndUpdateState("http://www.d.com/3", "D3");
  ValidateModel(back_model.get(), i++, 0);
  LoadURLAndUpdateState("http://www.e.com/1", "E1");
  ValidateModel(back_model.get(), i++, 0);
  LoadURLAndUpdateState("http://www.e.com/2", "E2");
  ValidateModel(back_model.get(), i++, 0);
  LoadURLAndUpdateState("http://www.e.com/3", "E3");
  ValidateModel(back_model.get(), i++, 0);
  LoadURLAndUpdateState("http://www.f.com/1", "F1"); // 15
  ValidateModel(back_model.get(), i++, 1);
  LoadURLAndUpdateState("http://www.f.com/2", "F2");
  ValidateModel(back_model.get(), i++, 1);
  LoadURLAndUpdateState("http://www.f.com/3", "F3");
  ValidateModel(back_model.get(), i++, 1);
  LoadURLAndUpdateState("http://www.g.com/1", "G1");
  ValidateModel(back_model.get(), i++, 2);
  LoadURLAndUpdateState("http://www.g.com/2", "G2");
  ValidateModel(back_model.get(), i++, 2);
  LoadURLAndUpdateState("http://www.g.com/3", "G3"); // 20
  ValidateModel(back_model.get(), i++, 2);
  LoadURLAndUpdateState("http://www.h.com/1", "H1");
  ValidateModel(back_model.get(), i++, 3);
  LoadURLAndUpdateState("http://www.h.com/2", "H2");
  ValidateModel(back_model.get(), i++, 3);
  LoadURLAndUpdateState("http://www.h.com/3", "H3");
  ValidateModel(back_model.get(), i++, 3);
  LoadURLAndUpdateState("http://www.i.com/1", "I1");
  ValidateModel(back_model.get(), i++, 4);
  LoadURLAndUpdateState("http://www.i.com/2", "I2"); // 25
  ValidateModel(back_model.get(), i++, 4);
  LoadURLAndUpdateState("http://www.i.com/3", "I3");
  ValidateModel(back_model.get(), i++, 4);
  LoadURLAndUpdateState("http://www.j.com/1", "J1");
  ValidateModel(back_model.get(), i++, 5);
  LoadURLAndUpdateState("http://www.j.com/2", "J2");
  ValidateModel(back_model.get(), i++, 5);
  LoadURLAndUpdateState("http://www.j.com/3", "J3");
  ValidateModel(back_model.get(), i++, 5);
  LoadURLAndUpdateState("http://www.k.com/1", "K1"); // 30
  ValidateModel(back_model.get(), i++, 6);
  LoadURLAndUpdateState("http://www.k.com/2", "K2");
  ValidateModel(back_model.get(), i++, 6);
  LoadURLAndUpdateState("http://www.k.com/3", "K3"); // 32
  ValidateModel(back_model.get(), i++, 6);

  // Check to see if the chapter stops have the right labels.
  int index = BackForwardMenuModel::kMaxHistoryItems + 1;
  EXPECT_EQ(ASCIIToUTF16(""), back_model->GetItemLabel(index++));  // separator.
  EXPECT_EQ(ASCIIToUTF16("F3"), back_model->GetItemLabel(index++));
  EXPECT_EQ(ASCIIToUTF16("E3"), back_model->GetItemLabel(index++));
  EXPECT_EQ(ASCIIToUTF16("D3"), back_model->GetItemLabel(index++));
  EXPECT_EQ(ASCIIToUTF16("C3"), back_model->GetItemLabel(index++));
  EXPECT_EQ(ASCIIToUTF16("B3"), back_model->GetItemLabel(index));  // max 5 chapter stops.
  EXPECT_EQ(ASCIIToUTF16(""), back_model->GetItemLabel(index + 1));  // separator.
  EXPECT_EQ(back_model->GetShowFullHistoryLabel(),
            back_model->GetItemLabel(index + 2));

  // If we go back two we should still see the same chapter stop at the end.
  GoBack();
  EXPECT_EQ(ASCIIToUTF16("B3"), back_model->GetItemLabel(index));
  GoBack();
  EXPECT_EQ(ASCIIToUTF16("B3"), back_model->GetItemLabel(index));
  // But if we go back again, it should change.
  GoBack();
  EXPECT_EQ(ASCIIToUTF16("A3"), back_model->GetItemLabel(index));
  GoBack();
  EXPECT_EQ(ASCIIToUTF16("A3"), back_model->GetItemLabel(index));
  GoBack();
  EXPECT_EQ(ASCIIToUTF16("A3"), back_model->GetItemLabel(index));
  GoBack();
  EXPECT_EQ(ASCIIToUTF16(""), back_model->GetItemLabel(index));  // It is now a separator.
  NavigateToOffset(6);  // Undo our position change.

  // Go back enough to make sure no chapter stops should appear.
  NavigateToOffset(-BackForwardMenuModel::kMaxHistoryItems);
  ValidateModel(forward_model.get(), BackForwardMenuModel::kMaxHistoryItems, 0);
  // Go forward (still no chapter stop)
  GoForward();
  ValidateModel(forward_model.get(),
                BackForwardMenuModel::kMaxHistoryItems - 1, 0);
  // Go back two (one chapter stop should show up)
  GoBack();
  GoBack();
  ValidateModel(forward_model.get(),
                BackForwardMenuModel::kMaxHistoryItems, 1);

  // Go to beginning.
  NavigateToIndex(0);

  // Check to see if the chapter stops have the right labels.
  index = BackForwardMenuModel::kMaxHistoryItems + 1;
  EXPECT_EQ(ASCIIToUTF16(""), forward_model->GetItemLabel(index++));    // separator.
  EXPECT_EQ(ASCIIToUTF16("E3"), forward_model->GetItemLabel(index++));
  EXPECT_EQ(ASCIIToUTF16("F3"), forward_model->GetItemLabel(index++));
  EXPECT_EQ(ASCIIToUTF16("G3"), forward_model->GetItemLabel(index++));
  EXPECT_EQ(ASCIIToUTF16("H3"), forward_model->GetItemLabel(index++));
  // max 5 chapter stops.
  EXPECT_EQ(ASCIIToUTF16("I3"), forward_model->GetItemLabel(index));
  EXPECT_EQ(ASCIIToUTF16(""), forward_model->GetItemLabel(index + 1));  // separator.
  EXPECT_EQ(forward_model->GetShowFullHistoryLabel(),
      forward_model->GetItemLabel(index + 2));

  // If we advance one we should still see the same chapter stop at the end.
  GoForward();
  EXPECT_EQ(ASCIIToUTF16("I3"), forward_model->GetItemLabel(index));
  // But if we advance one again, it should change.
  GoForward();
  EXPECT_EQ(ASCIIToUTF16("J3"), forward_model->GetItemLabel(index));
  GoForward();
  EXPECT_EQ(ASCIIToUTF16("J3"), forward_model->GetItemLabel(index));
  GoForward();
  EXPECT_EQ(ASCIIToUTF16("J3"), forward_model->GetItemLabel(index));
  GoForward();
  EXPECT_EQ(ASCIIToUTF16("K3"), forward_model->GetItemLabel(index));

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
  NavigateToIndex(0);  // Go to A1;
  LoadURLAndUpdateState("http://www.b.com/1", "B1");
  EXPECT_EQ(0, back_model->GetIndexOfNextChapterStop(1, false));
  EXPECT_EQ(1, back_model->GetIndexOfNextChapterStop(0, true));

  // Now see if it counts 'www.x.com' and 'mail.x.com' as same domain, which
  // it should.
  NavigateToIndex(0);  // Go to A1.
  LoadURLAndUpdateState("http://mail.a.com/2", "A2-mai");
  LoadURLAndUpdateState("http://www.b.com/1", "B1");
  LoadURLAndUpdateState("http://mail.b.com/2", "B2-mai");
  LoadURLAndUpdateState("http://new.site.com", "new");
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
