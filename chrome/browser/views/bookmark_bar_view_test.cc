// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_util.h"
#include "chrome/browser/automation/ui_controls.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/page_navigator.h"
#include "chrome/browser/views/bookmark_bar_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/test/testing_profile.h"
#include "chrome/test/interactive_ui/view_event_test_base.h"
#include "chrome/views/chrome_menu.h"
#include "chrome/views/text_button.h"
#include "chrome/views/window/window.h"

namespace {

// PageNavigator implementation that records the URL.
class TestingPageNavigator : public PageNavigator {
 public:
  virtual void OpenURL(const GURL& url, const GURL& referrer,
                       WindowOpenDisposition disposition,
                       PageTransition::Type transition) {
    url_ = url;
  }

  GURL url_;
};

}  // namespace

// Base class for event generating bookmark view tests. These test are intended
// to exercise ChromeMenus, but that's easier done with BookmarkBarView rather
// than ChromeMenu itself.
//
// SetUp creates a bookmark model with the following structure.
// All folders are in upper case, all URLs in lower case.
// F1
//   f1a
//   F11
//     f11a
//   *
// a
// b
// c
// d
// OTHER
//   oa
//   OF
//     ofa
//     ofb
//   OF2
//     of2a
//     of2b
//
// * if CreateBigMenu returns return true, 100 menu items are created here with
//   the names f1-f100.
//
// Subclasses should be sure and invoke super's implementation of SetUp and
// TearDown.
class BookmarkBarViewEventTestBase : public ViewEventTestBase {
 public:
  BookmarkBarViewEventTestBase()
      : ViewEventTestBase(), bb_view_(NULL), model_(NULL) {
  }

  virtual void SetUp() {
    views::MenuItemView::allow_task_nesting_during_run_ = true;
    BookmarkBarView::testing_ = true;

    profile_.reset(new TestingProfile());
    profile_->set_has_history_service(true);
    profile_->CreateBookmarkModel(true);
    profile_->GetPrefs()->SetBoolean(prefs::kShowBookmarkBar, true);

    model_ = profile_->GetBookmarkModel();
    model_->ClearStore();

    bb_view_ = new BookmarkBarView(profile_.get(), NULL);
    bb_view_->SetPageNavigator(&navigator_);

    AddTestData(CreateBigMenu());

    // Calculate the preferred size so that one button doesn't fit, which
    // triggers the overflow button to appear.
    //
    // BookmarkBarView::Layout does nothing if the parent is NULL and
    // GetPreferredSize hard codes a width of 1. For that reason we add the
    // BookmarkBarView to a dumby view as the parent.
    //
    // This code looks a bit hacky, but I've written it so that it shouldn't
    // be dependant upon any of the layout code in BookmarkBarView. Instead
    // we brute force search for a size that triggers the overflow button.
    views::View tmp_parent;

    tmp_parent.AddChildView(bb_view_);

    bb_view_pref_ = bb_view_->GetPreferredSize();
    bb_view_pref_.set_width(1000);
    views::TextButton* button = bb_view_->GetBookmarkButton(4);
    while (button->IsVisible()) {
      bb_view_pref_.set_width(bb_view_pref_.width() - 25);
      bb_view_->SetBounds(0, 0, bb_view_pref_.width(), bb_view_pref_.height());
      bb_view_->Layout();
    }

    tmp_parent.RemoveChildView(bb_view_);

    ViewEventTestBase::SetUp();
  }

  virtual void TearDown() {
    views::MenuItemView::allow_task_nesting_during_run_ = false;
    ViewEventTestBase::TearDown();
    BookmarkBarView::testing_ = false;
  }

 protected:
  virtual views::View* CreateContentsView() {
    return bb_view_;
  }

  virtual gfx::Size GetPreferredSize() { return bb_view_pref_; }

  // See comment above class description for what this does.
  virtual bool CreateBigMenu() { return false; }

  BookmarkModel* model_;
  BookmarkBarView* bb_view_;
  TestingPageNavigator navigator_;

 private:
  void AddTestData(bool big_menu) {
    std::string test_base = "file:///c:/tmp/";

    BookmarkNode* f1 = model_->AddGroup(model_->GetBookmarkBarNode(), 0, L"F1");
    model_->AddURL(f1, 0, L"f1a", GURL(test_base + "f1a"));
    BookmarkNode* f11 = model_->AddGroup(f1, 1, L"F11");
    model_->AddURL(f11, 0, L"f11a", GURL(test_base + "f11a"));
    if (big_menu) {
      for (int i = 1; i <= 100; ++i) {
        model_->AddURL(f1, i + 1, L"f" + IntToWString(i),
                       GURL(test_base + "f" + IntToString(i)));
      }
    }
    model_->AddURL(model_->GetBookmarkBarNode(), 1, L"a",
                   GURL(test_base + "a"));
    model_->AddURL(model_->GetBookmarkBarNode(), 2, L"b",
                   GURL(test_base + "b"));
    model_->AddURL(model_->GetBookmarkBarNode(), 3, L"c",
                   GURL(test_base + "c"));
    model_->AddURL(model_->GetBookmarkBarNode(), 4, L"d",
                   GURL(test_base + "d"));
    model_->AddURL(model_->other_node(), 0, L"oa", GURL(test_base + "oa"));
    BookmarkNode* of = model_->AddGroup(model_->other_node(), 1, L"OF");
    model_->AddURL(of, 0, L"ofa", GURL(test_base + "ofa"));
    model_->AddURL(of, 1, L"ofb", GURL(test_base + "ofb"));
    BookmarkNode* of2 = model_->AddGroup(model_->other_node(), 2, L"OF2");
    model_->AddURL(of2, 0, L"of2a", GURL(test_base + "of2a"));
    model_->AddURL(of2, 1, L"of2b", GURL(test_base + "of2b"));
  }

  gfx::Size bb_view_pref_;
  scoped_ptr<TestingProfile> profile_;
};

// Clicks on first menu, makes sure button is depressed. Moves mouse to first
// child, clicks it and makes sure a navigation occurs.
class BookmarkBarViewTest1 : public BookmarkBarViewEventTestBase {
 protected:
  virtual void DoTestOnMessageLoop() {
    // Move the mouse to the first folder on the bookmark bar and press the
    // mouse.
    views::TextButton* button = bb_view_->GetBookmarkButton(0);
    ui_controls::MoveMouseToCenterAndPress(button, ui_controls::LEFT,
        ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest1::Step2));
  }

 private:
  void Step2() {
    // Menu should be showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());

    // Button should be depressed.
    views::TextButton* button = bb_view_->GetBookmarkButton(0);
    ASSERT_TRUE(button->state() == views::CustomButton::BS_PUSHED);

    // Click on the 2nd menu item (A URL).
    ASSERT_TRUE(menu->GetSubmenu());

    views::MenuItemView* menu_to_select =
        menu->GetSubmenu()->GetMenuItemAt(0);
    ui_controls::MoveMouseToCenterAndPress(menu_to_select, ui_controls::LEFT,
        ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest1::Step3));
  }

  void Step3() {
    // We should have navigated to URL f1a.
    ASSERT_TRUE(navigator_.url_ ==
                model_->GetBookmarkBarNode()->GetChild(0)->GetChild(0)->
                GetURL());

    // Make sure button is no longer pushed.
    views::TextButton* button = bb_view_->GetBookmarkButton(0);
    ASSERT_TRUE(button->state() == views::CustomButton::BS_NORMAL);

    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu == NULL || !menu->GetSubmenu()->IsShowing());

    Done();
  }
};

VIEW_TEST(BookmarkBarViewTest1, Basic)

// Brings up menu, clicks on empty space and make sure menu hides.
class BookmarkBarViewTest2 : public BookmarkBarViewEventTestBase {
 protected:
  virtual void DoTestOnMessageLoop() {
    // Move the mouse to the first folder on the bookmark bar and press the
    // mouse.
    views::TextButton* button = bb_view_->GetBookmarkButton(0);
    ui_controls::MoveMouseToCenterAndPress(button, ui_controls::LEFT,
        ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest2::Step2));
  }

 private:
  void Step2() {
    // Menu should be showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu != NULL && menu->GetSubmenu()->IsShowing());

    // Click on 0x0, which should trigger closing menu.
    // NOTE: this code assume there is a left margin, which is currently
    // true. If that changes, this code will need to find another empty space
    // to press the mouse on.
    gfx::Point mouse_loc;
    views::View::ConvertPointToScreen(bb_view_, &mouse_loc);
    ui_controls::SendMouseMove(0, 0);
    ui_controls::SendMouseEventsNotifyWhenDone(
        ui_controls::LEFT, ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest2::Step3));
  }

  void Step3() {
    // The menu shouldn't be showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu == NULL || !menu->GetSubmenu()->IsShowing());

    // Make sure button is no longer pushed.
    views::TextButton* button = bb_view_->GetBookmarkButton(0);
    ASSERT_TRUE(button->state() == views::CustomButton::BS_NORMAL);

    window_->Activate();

    Done();
  }
};

VIEW_TEST(BookmarkBarViewTest2, HideOnDesktopClick)

// Brings up menu. Moves over child to make sure submenu appears, moves over
// another child and make sure next menu appears.
class BookmarkBarViewTest3 : public BookmarkBarViewEventTestBase {
 protected:
  virtual void DoTestOnMessageLoop() {
    // Move the mouse to the first folder on the bookmark bar and press the
    // mouse.
    views::TextButton* button = bb_view_->other_bookmarked_button();
    ui_controls::MoveMouseToCenterAndPress(button, ui_controls::LEFT,
        ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest3::Step2));
  }

 private:
  void Step2() {
    // Menu should be showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());

    views::MenuItemView* child_menu =
        menu->GetSubmenu()->GetMenuItemAt(1);
    ASSERT_TRUE(child_menu != NULL);

    // Click on second child, which has a submenu.
    ui_controls::MoveMouseToCenterAndPress(child_menu, ui_controls::LEFT,
        ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest3::Step3));
  }

  void Step3() {
    // Make sure sub menu is showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    views::MenuItemView* child_menu =
        menu->GetSubmenu()->GetMenuItemAt(1);
    ASSERT_TRUE(child_menu->GetSubmenu() != NULL);
    ASSERT_TRUE(child_menu->GetSubmenu()->IsShowing());

    // Click on third child, which has a submenu too.
    child_menu = menu->GetSubmenu()->GetMenuItemAt(2);
    ASSERT_TRUE(child_menu != NULL);
    ui_controls::MoveMouseToCenterAndPress(child_menu, ui_controls::LEFT,
        ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest3::Step4));
  }

  void Step4() {
    // Make sure sub menu we first clicked isn't showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    views::MenuItemView* child_menu =
        menu->GetSubmenu()->GetMenuItemAt(1);
    ASSERT_TRUE(child_menu->GetSubmenu() != NULL);
    ASSERT_FALSE(child_menu->GetSubmenu()->IsShowing());

    // And submenu we last clicked is showing.
    child_menu = menu->GetSubmenu()->GetMenuItemAt(2);
    ASSERT_TRUE(child_menu != NULL);
    ASSERT_TRUE(child_menu->GetSubmenu()->IsShowing());

    // Nothing should have been selected.
    ASSERT_TRUE(navigator_.url_ == GURL());

    // Hide menu.
    menu->GetMenuController()->Cancel(true);

    Done();
  }
};

VIEW_TEST(BookmarkBarViewTest3, Submenus)

// Tests context menus by way of opening a context menu for a bookmark,
// then right clicking to get context menu and selecting the first menu item
// (open).
class BookmarkBarViewTest4 : public BookmarkBarViewEventTestBase {
 protected:
  virtual void DoTestOnMessageLoop() {
    // Move the mouse to the first folder on the bookmark bar and press the
    // mouse.
    views::TextButton* button = bb_view_->other_bookmarked_button();
    ui_controls::MoveMouseToCenterAndPress(button, ui_controls::LEFT,
        ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest4::Step2));
  }

 private:
  void Step2() {
    // Menu should be showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());

    views::MenuItemView* child_menu =
        menu->GetSubmenu()->GetMenuItemAt(0);
    ASSERT_TRUE(child_menu != NULL);

    // Right click on the first child to get its context menu.
    ui_controls::MoveMouseToCenterAndPress(child_menu, ui_controls::RIGHT,
        ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest4::Step3));
  }

  void Step3() {
    // Make sure the context menu is showing.
    views::MenuItemView* menu = bb_view_->GetContextMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu());
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());

    // Select the first menu item (open).
    ui_controls::MoveMouseToCenterAndPress(menu->GetSubmenu()->GetMenuItemAt(0),
        ui_controls::LEFT, ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest4::Step4));
  }

  void Step4() {
    ASSERT_TRUE(navigator_.url_ ==
                model_->other_node()->GetChild(0)->GetURL());

    Done();
  }
};

VIEW_TEST(BookmarkBarViewTest4, ContextMenus)

// Tests drag and drop within the same menu.
class BookmarkBarViewTest5 : public BookmarkBarViewEventTestBase {
 protected:
  virtual void DoTestOnMessageLoop() {
    url_dragging_ =
        model_->GetBookmarkBarNode()->GetChild(0)->GetChild(0)->GetURL();

    // Move the mouse to the first folder on the bookmark bar and press the
    // mouse.
    views::TextButton* button = bb_view_->GetBookmarkButton(0);
    ui_controls::MoveMouseToCenterAndPress(button, ui_controls::LEFT,
        ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest5::Step2));
  }

 private:
  void Step2() {
    // Menu should be showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());

    views::MenuItemView* child_menu =
        menu->GetSubmenu()->GetMenuItemAt(0);
    ASSERT_TRUE(child_menu != NULL);

    // Move mouse to center of menu and press button.
    ui_controls::MoveMouseToCenterAndPress(child_menu, ui_controls::LEFT,
        ui_controls::DOWN,
        CreateEventTask(this, &BookmarkBarViewTest5::Step3));
  }

  void Step3() {
    views::MenuItemView* target_menu =
        bb_view_->GetMenu()->GetSubmenu()->GetMenuItemAt(1);
    gfx::Point loc(1, target_menu->height() - 1);
    views::View::ConvertPointToScreen(target_menu, &loc);

    // Start a drag.
    ui_controls::SendMouseMoveNotifyWhenDone(loc.x() + 10, loc.y(),
        CreateEventTask(this, &BookmarkBarViewTest5::Step4));

    // See comment above this method as to why we do this.
    ScheduleMouseMoveInBackground(loc.x(), loc.y());
  }

  void Step4() {
    // Drop the item so that it's now the second item.
   views::MenuItemView* target_menu =
        bb_view_->GetMenu()->GetSubmenu()->GetMenuItemAt(1);
   gfx::Point loc(1, target_menu->height() - 1);
    views::View::ConvertPointToScreen(target_menu, &loc);
    ui_controls::SendMouseMove(loc.x(), loc.y());

    ui_controls::SendMouseEventsNotifyWhenDone(ui_controls::LEFT,
        ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest5::Step5));
  }

  void Step5() {
    GURL url = model_->GetBookmarkBarNode()->GetChild(0)->GetChild(1)->GetURL();
    ASSERT_TRUE(url == url_dragging_);
    Done();
  }

  GURL url_dragging_;
};

VIEW_TEST(BookmarkBarViewTest5, DND)

// Tests holding mouse down on overflow button, dragging such that menu pops up
// then selecting an item.
class BookmarkBarViewTest6 : public BookmarkBarViewEventTestBase {
 protected:
  virtual void DoTestOnMessageLoop() {
    // Press the mouse button on the overflow button. Don't release it though.
    views::TextButton* button = bb_view_->overflow_button();
    ui_controls::MoveMouseToCenterAndPress(button, ui_controls::LEFT,
        ui_controls::DOWN, CreateEventTask(this, &BookmarkBarViewTest6::Step2));
  }

 private:
  void Step2() {
    // Menu should be showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());

    views::MenuItemView* child_menu =
        menu->GetSubmenu()->GetMenuItemAt(0);
    ASSERT_TRUE(child_menu != NULL);

    // Move mouse to center of menu and release mouse.
    ui_controls::MoveMouseToCenterAndPress(child_menu, ui_controls::LEFT,
        ui_controls::UP, CreateEventTask(this, &BookmarkBarViewTest6::Step3));
  }

  void Step3() {
    ASSERT_TRUE(navigator_.url_ ==
                model_->GetBookmarkBarNode()->GetChild(4)->GetURL());
    Done();
  }

  GURL url_dragging_;
};

VIEW_TEST(BookmarkBarViewTest6, OpenMenuOnClickAndHold)

// Tests drag and drop to different menu.
class BookmarkBarViewTest7 : public BookmarkBarViewEventTestBase {
 protected:
  virtual void DoTestOnMessageLoop() {
    url_dragging_ =
        model_->GetBookmarkBarNode()->GetChild(0)->GetChild(0)->GetURL();

    // Move the mouse to the first folder on the bookmark bar and press the
    // mouse.
    views::TextButton* button = bb_view_->GetBookmarkButton(0);
    ui_controls::MoveMouseToCenterAndPress(button, ui_controls::LEFT,
        ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest7::Step2));
  }

 private:
  void Step2() {
    // Menu should be showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());

    views::MenuItemView* child_menu =
        menu->GetSubmenu()->GetMenuItemAt(0);
    ASSERT_TRUE(child_menu != NULL);

    // Move mouse to center of menu and press button.
    ui_controls::MoveMouseToCenterAndPress(child_menu, ui_controls::LEFT,
        ui_controls::DOWN,
        CreateEventTask(this, &BookmarkBarViewTest7::Step3));
  }

  void Step3() {
    // Drag over other button.
    views::TextButton* other_button =
        bb_view_->other_bookmarked_button();
    gfx::Point loc(other_button->width() / 2, other_button->height() / 2);
    views::View::ConvertPointToScreen(other_button, &loc);

    // Start a drag.
    ui_controls::SendMouseMoveNotifyWhenDone(loc.x() + 10, loc.y(),
        NewRunnableMethod(this, &BookmarkBarViewTest7::Step4));

    // See comment above this method as to why we do this.
    ScheduleMouseMoveInBackground(loc.x(), loc.y());
  }

  void Step4() {
    views::MenuItemView* drop_menu = bb_view_->GetDropMenu();
    ASSERT_TRUE(drop_menu != NULL);
    ASSERT_TRUE(drop_menu->GetSubmenu()->IsShowing());

    views::MenuItemView* target_menu =
        drop_menu->GetSubmenu()->GetMenuItemAt(0);
    gfx::Point loc(1, 1);
    views::View::ConvertPointToScreen(target_menu, &loc);
    ui_controls::SendMouseMove(loc.x(), loc.y());
    ui_controls::SendMouseEventsNotifyWhenDone(
        ui_controls::LEFT, ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest7::Step5));
  }

  void Step5() {
    ASSERT_TRUE(model_->other_node()->GetChild(0)->GetURL() == url_dragging_);
    Done();
  }

  GURL url_dragging_;
};

VIEW_TEST(BookmarkBarViewTest7, DNDToDifferentMenu)

// Drags from one menu to next so that original menu closes, then back to
// original menu.
class BookmarkBarViewTest8 : public BookmarkBarViewEventTestBase {
 protected:
  virtual void DoTestOnMessageLoop() {
    url_dragging_ =
        model_->GetBookmarkBarNode()->GetChild(0)->GetChild(0)->GetURL();

    // Move the mouse to the first folder on the bookmark bar and press the
    // mouse.
    views::TextButton* button = bb_view_->GetBookmarkButton(0);
    ui_controls::MoveMouseToCenterAndPress(button, ui_controls::LEFT,
        ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest8::Step2));
  }

 private:
  void Step2() {
    // Menu should be showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());

    views::MenuItemView* child_menu =
        menu->GetSubmenu()->GetMenuItemAt(0);
    ASSERT_TRUE(child_menu != NULL);

    // Move mouse to center of menu and press button.
    ui_controls::MoveMouseToCenterAndPress(child_menu, ui_controls::LEFT,
        ui_controls::DOWN,
        CreateEventTask(this, &BookmarkBarViewTest8::Step3));
  }

  void Step3() {
    // Drag over other button.
    views::TextButton* other_button =
        bb_view_->other_bookmarked_button();
    gfx::Point loc(other_button->width() / 2, other_button->height() / 2);
    views::View::ConvertPointToScreen(other_button, &loc);

    // Start a drag.
    ui_controls::SendMouseMoveNotifyWhenDone(loc.x() + 10, loc.y(),
        NewRunnableMethod(this, &BookmarkBarViewTest8::Step4));

    // See comment above this method as to why we do this.
    ScheduleMouseMoveInBackground(loc.x(), loc.y());
  }

  void Step4() {
    views::MenuItemView* drop_menu = bb_view_->GetDropMenu();
    ASSERT_TRUE(drop_menu != NULL);
    ASSERT_TRUE(drop_menu->GetSubmenu()->IsShowing());

    // Now drag back over first menu.
    views::TextButton* button = bb_view_->GetBookmarkButton(0);
    gfx::Point loc(button->width() / 2, button->height() / 2);
    views::View::ConvertPointToScreen(button, &loc);
    ui_controls::SendMouseMoveNotifyWhenDone(loc.x(), loc.y(),
        NewRunnableMethod(this, &BookmarkBarViewTest8::Step5));
  }

  void Step5() {
    // Drop on folder F11.
    views::MenuItemView* drop_menu = bb_view_->GetDropMenu();
    ASSERT_TRUE(drop_menu != NULL);
    ASSERT_TRUE(drop_menu->GetSubmenu()->IsShowing());

    views::MenuItemView* target_menu =
        drop_menu->GetSubmenu()->GetMenuItemAt(1);
    ui_controls::MoveMouseToCenterAndPress(
        target_menu, ui_controls::LEFT, ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest8::Step6));
  }

  void Step6() {
    // Make sure drop was processed.
    GURL final_url = model_->GetBookmarkBarNode()->GetChild(0)->GetChild(0)->
        GetChild(1)->GetURL();
    ASSERT_TRUE(final_url == url_dragging_);
    Done();
  }

  GURL url_dragging_;
};

VIEW_TEST(BookmarkBarViewTest8, DNDBackToOriginatingMenu)

// Moves the mouse over the scroll button and makes sure we get scrolling.
class BookmarkBarViewTest9 : public BookmarkBarViewEventTestBase {
 protected:
  virtual bool CreateBigMenu() { return true; }

  virtual void DoTestOnMessageLoop() {
    // Move the mouse to the first folder on the bookmark bar and press the
    // mouse.
    views::TextButton* button = bb_view_->GetBookmarkButton(0);
    ui_controls::MoveMouseToCenterAndPress(button, ui_controls::LEFT,
        ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest9::Step2));
  }

 private:
  void Step2() {
    // Menu should be showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());

    first_menu_ = menu->GetSubmenu()->GetMenuItemAt(0);
    gfx::Point menu_loc;
    views::View::ConvertPointToScreen(first_menu_, &menu_loc);
    start_y_ = menu_loc.y();

    // Move the mouse over the scroll button.
    views::View* scroll_container = menu->GetSubmenu()->GetParent();
    ASSERT_TRUE(scroll_container != NULL);
    scroll_container = scroll_container->GetParent();
    ASSERT_TRUE(scroll_container != NULL);
    views::View* scroll_down_button = scroll_container->GetChildViewAt(1);
    ASSERT_TRUE(scroll_down_button);
    gfx::Point loc(scroll_down_button->width() / 2,
                   scroll_down_button->height() / 2);
    views::View::ConvertPointToScreen(scroll_down_button, &loc);
    ui_controls::SendMouseMoveNotifyWhenDone(
        loc.x(), loc.y(), CreateEventTask(this, &BookmarkBarViewTest9::Step3));
  }

  void Step3() {
    MessageLoop::current()->PostDelayedTask(FROM_HERE,
        NewRunnableMethod(this, &BookmarkBarViewTest9::Step4), 200);
  }

  void Step4() {
    gfx::Point menu_loc;
    views::View::ConvertPointToScreen(first_menu_, &menu_loc);
    ASSERT_NE(start_y_, menu_loc.y());

    // Hide menu.
    bb_view_->GetMenu()->GetMenuController()->Cancel(true);

    Done();
  }

  int start_y_;
  views::MenuItemView* first_menu_;
};

VIEW_TEST(BookmarkBarViewTest9, ScrollButtonScrolls)

// Tests up/down/left/enter key messages.
class BookmarkBarViewTest10 : public BookmarkBarViewEventTestBase {
 protected:
  virtual void DoTestOnMessageLoop() {
    // Move the mouse to the first folder on the bookmark bar and press the
    // mouse.
    views::TextButton* button = bb_view_->GetBookmarkButton(0);
    ui_controls::MoveMouseToCenterAndPress(button, ui_controls::LEFT,
        ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest10::Step2));
  }

 private:
  void Step2() {
    // Menu should be showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());

    // Send a down event, which should select the first item.
    ui_controls::SendKeyPressNotifyWhenDone(
        VK_DOWN, false, false, false,
        CreateEventTask(this, &BookmarkBarViewTest10::Step3));
  }

  void Step3() {
    // Make sure menu is showing and item is selected.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());
    ASSERT_TRUE(menu->GetSubmenu()->GetMenuItemAt(0)->IsSelected());

    // Send a key down event, which should select the next item.
    ui_controls::SendKeyPressNotifyWhenDone(
        VK_DOWN, false, false, false,
        CreateEventTask(this, &BookmarkBarViewTest10::Step4));
  }

  void Step4() {
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());
    ASSERT_FALSE(menu->GetSubmenu()->GetMenuItemAt(0)->IsSelected());
    ASSERT_TRUE(menu->GetSubmenu()->GetMenuItemAt(1)->IsSelected());

    // Send a right arrow to force the menu to open.
    ui_controls::SendKeyPressNotifyWhenDone(
        VK_RIGHT, false, false, false,
        CreateEventTask(this, &BookmarkBarViewTest10::Step5));
  }

  void Step5() {
    // Make sure the submenu is showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());
    views::MenuItemView* submenu = menu->GetSubmenu()->GetMenuItemAt(1);
    ASSERT_TRUE(submenu->IsSelected());
    ASSERT_TRUE(submenu->GetSubmenu());
    ASSERT_TRUE(submenu->GetSubmenu()->IsShowing());

    // Send a left arrow to close the submenu.
    ui_controls::SendKeyPressNotifyWhenDone(
        VK_LEFT, false, false, false,
        CreateEventTask(this, &BookmarkBarViewTest10::Step6));
  }

  void Step6() {
    // Make sure the submenu is showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());
    views::MenuItemView* submenu = menu->GetSubmenu()->GetMenuItemAt(1);
    ASSERT_TRUE(submenu->IsSelected());
    ASSERT_TRUE(!submenu->GetSubmenu() || !submenu->GetSubmenu()->IsShowing());

    // Send a down arrow to wrap back to f1a
    ui_controls::SendKeyPressNotifyWhenDone(
        VK_DOWN, false, false, false,
        CreateEventTask(this, &BookmarkBarViewTest10::Step7));
  }

  void Step7() {
    // Make sure menu is showing and item is selected.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());
    ASSERT_TRUE(menu->GetSubmenu()->GetMenuItemAt(0)->IsSelected());

    // Send enter, which should select the item.
    ui_controls::SendKeyPressNotifyWhenDone(
        VK_RETURN, false, false, false,
        CreateEventTask(this, &BookmarkBarViewTest10::Step8));
  }

  void Step8() {
    ASSERT_TRUE(
        model_->GetBookmarkBarNode()->GetChild(0)->GetChild(0)->GetURL() ==
        navigator_.url_);
    Done();
  }
};

VIEW_TEST(BookmarkBarViewTest10, KeyEvents)

// Make sure the menu closes with the following sequence: show menu, show
// context menu, close context menu (via escape), then click else where. This
// effectively verifies we maintain mouse capture after the context menu is
// hidden.
class BookmarkBarViewTest11 : public BookmarkBarViewEventTestBase {
 protected:
  virtual void DoTestOnMessageLoop() {
    // Move the mouse to the first folder on the bookmark bar and press the
    // mouse.
    views::TextButton* button = bb_view_->other_bookmarked_button();
    ui_controls::MoveMouseToCenterAndPress(button, ui_controls::LEFT,
        ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest11::Step2));
  }

 private:
  void Step2() {
    // Menu should be showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());

    views::MenuItemView* child_menu =
        menu->GetSubmenu()->GetMenuItemAt(0);
    ASSERT_TRUE(child_menu != NULL);

    // Right click on the first child to get its context menu.
    ui_controls::MoveMouseToCenterAndPress(child_menu, ui_controls::RIGHT,
        ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest11::Step3));
  }

  void Step3() {
    // Send escape so that the context menu hides.
    ui_controls::SendKeyPressNotifyWhenDone(VK_ESCAPE, false, false, false,
        CreateEventTask(this, &BookmarkBarViewTest11::Step4));
  }

  void Step4() {
    // Make sure the context menu is no longer showing.
    views::MenuItemView* menu = bb_view_->GetContextMenu();
    ASSERT_TRUE(!menu || !menu->GetSubmenu() ||
                !menu->GetSubmenu()->IsShowing());

    // But the menu should be showing.
    menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu && menu->GetSubmenu() && menu->GetSubmenu()->IsShowing());

    // Now click on empty space.
    gfx::Point mouse_loc;
    views::View::ConvertPointToScreen(bb_view_, &mouse_loc);
    ui_controls::SendMouseMove(mouse_loc.x(), mouse_loc.y());
    ui_controls::SendMouseEventsNotifyWhenDone(
        ui_controls::LEFT, ui_controls::UP | ui_controls::DOWN,
        CreateEventTask(this, &BookmarkBarViewTest11::Step5));
  }

  void Step5() {
    // Make sure the menu is not showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(!menu || !menu->GetSubmenu() ||
                !menu->GetSubmenu()->IsShowing());
    Done();
  }
};

VIEW_TEST(BookmarkBarViewTest11, CloseMenuAfterClosingContextMenu)

// Tests showing a modal dialog from a context menu.
class BookmarkBarViewTest12 : public BookmarkBarViewEventTestBase {
 protected:
  virtual void DoTestOnMessageLoop() {
    // Open up the other folder.
    views::TextButton* button = bb_view_->other_bookmarked_button();
    ui_controls::MoveMouseToCenterAndPress(button, ui_controls::LEFT,
        ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest12::Step2));
    bookmark_utils::num_urls_before_prompting = 1;
  }

  ~BookmarkBarViewTest12() {
    bookmark_utils::num_urls_before_prompting = 15;
  }

 private:
  void Step2() {
    // Menu should be showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());

    views::MenuItemView* child_menu =
        menu->GetSubmenu()->GetMenuItemAt(1);
    ASSERT_TRUE(child_menu != NULL);

    // Right click on the second child (a folder) to get its context menu.
    ui_controls::MoveMouseToCenterAndPress(child_menu, ui_controls::RIGHT,
        ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest12::Step3));
  }

  void Step3() {
    // Make sure the context menu is showing.
    views::MenuItemView* menu = bb_view_->GetContextMenu();
    ASSERT_TRUE(menu && menu->GetSubmenu() && menu->GetSubmenu()->IsShowing());

    // Select the first item in the context menu (open all).
    views::MenuItemView* child_menu =
        menu->GetSubmenu()->GetMenuItemAt(0);
    ASSERT_TRUE(child_menu != NULL);
    ui_controls::MoveMouseToCenterAndPress(child_menu, ui_controls::LEFT,
        ui_controls::DOWN | ui_controls::UP, NULL);

    // Delay until we send tab, otherwise the message box doesn't appear
    // correctly.
    MessageLoop::current()->PostDelayedTask(FROM_HERE,
        CreateEventTask(this, &BookmarkBarViewTest12::Step4), 1000);
  }

  void Step4() {
    // Press tab to give focus to the cancel button.
    ui_controls::SendKeyPressNotifyWhenDone(VK_TAB, false, false, false,
        NULL);

    // For some reason return isn't processed correctly unless we delay.
    MessageLoop::current()->PostDelayedTask(FROM_HERE,
        CreateEventTask(this, &BookmarkBarViewTest12::Step5), 1000);
  }

  void Step5() {
    // And press enter so that the cancel button is selected.
    ui_controls::SendKeyPressNotifyWhenDone(VK_RETURN, false, false, false,
        CreateEventTask(this, &BookmarkBarViewTest12::Step6));
  }

  void Step6() {
    // Do a delayed task to give the dialog time to exit.
    MessageLoop::current()->PostTask(
        FROM_HERE, CreateEventTask(this, &BookmarkBarViewTest12::Step7));
  }

  void Step7() {
    Done();
  }
};

VIEW_TEST(BookmarkBarViewTest12, CloseWithModalDialog)
