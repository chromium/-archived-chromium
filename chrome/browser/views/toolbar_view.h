// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_TOOLBAR_VIEW_H__
#define CHROME_BROWSER_VIEWS_TOOLBAR_VIEW_H__

#include <vector>

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/back_forward_menu_model.h"
#include "chrome/browser/controller.h"
#include "chrome/browser/encoding_menu_controller_delegate.h"
#include "chrome/browser/user_data_manager.h"
#include "chrome/browser/views/dom_view.h"
#include "chrome/browser/views/go_button.h"
#include "chrome/browser/views/location_bar_view.h"
#include "chrome/common/pref_member.h"
#include "chrome/views/menu.h"
#include "chrome/views/menu_button.h"
#include "chrome/views/view.h"
#include "chrome/views/view_menu_delegate.h"

class Browser;
class Profile;
class ToolbarStarToggle;

///////////////////////////////////////////////////////////////////////////////
//
// BrowserToolbarView class
//
//  The BrowserToolbarView is responsible for constructing the content of and
//  rendering the Toolbar used in the Browser Window
//
///////////////////////////////////////////////////////////////////////////////
class BrowserToolbarView : public views::View,
                           public EncodingMenuControllerDelegate,
                           public views::ViewMenuDelegate,
                           public views::DragController,
                           public LocationBarView::Delegate,
                           public NotificationObserver,
                           public GetProfilesHelper::Delegate {
 public:
  BrowserToolbarView(CommandController* controller, Browser* browser);
  virtual ~BrowserToolbarView();

  // Create the contents of the Browser Toolbar
  void Init(Profile* profile);

  // views::View
  virtual void Layout();
  virtual void DidGainFocus();
  virtual void WillLoseFocus();
  virtual bool OnKeyPressed(const views::KeyEvent& e);
  virtual bool OnKeyReleased(const views::KeyEvent& e);
  virtual gfx::Size GetPreferredSize();

  // Overridden from EncodingMenuControllerDelegate:
  virtual bool IsItemChecked(int id) const;

  // Overridden from Menu::BaseControllerDelegate:
  virtual bool GetAcceleratorInfo(int id, views::Accelerator* accel);

  // views::MenuDelegate
  virtual void RunMenu(views::View* source, const CPoint& pt, HWND hwnd);

  // GetProfilesHelper::Delegate method.
  virtual void OnGetProfilesDone(const std::vector<std::wstring>& profiles);

  // Sets the profile which is active on the currently-active tab.
  void SetProfile(Profile* profile);
  Profile* profile() { return profile_; }

  ToolbarStarToggle* star_button() { return star_; }

  GoButton* GetGoButton() { return go_; }

  LocationBarView* GetLocationBarView() const { return location_bar_; }

  // Updates the toolbar (and transitively the location bar) with the states of
  // the specified |tab|.  If |should_restore_state| is true, we're switching
  // (back?) to this tab and should restore any previous location bar state
  // (such as user editing) as well.
  void Update(TabContents* tab, bool should_restore_state);

  void OnInputInProgress(bool in_progress);

  // Returns the MSAA role of the current view. The role is what assistive
  // technologies (ATs) use to determine what behavior to expect from a given
  // control.
  bool GetAccessibleRole(VARIANT* role);

  // Returns a brief, identifying string, containing a unique, readable name.
  bool GetAccessibleName(std::wstring* name);

  // Assigns an accessible string name.
  void SetAccessibleName(const std::wstring& name);

  // Returns the index of the next view of the toolbar, starting from the given
  // view index (skipping the location bar), in the given navigation direction
  // (nav_left true means navigation right to left, and vice versa). -1 finds
  // first accessible child, based on the above policy.
  int GetNextAccessibleViewIndex(int view_index, bool nav_left);

  views::View* acc_focused_view() {
    return acc_focused_view_;
  }

  void set_acc_focused_view(views::View* acc_focused_view) {
    acc_focused_view_ = acc_focused_view;
  }

  // Returns the selected tab.
  virtual TabContents* GetTabContents();

  Browser* browser() { return browser_; }

 private:
  // NotificationObserver
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // DragController methods for the star button. These allow the drag if the
  // user hasn't edited the text, the url is valid and should be displayed.
  virtual void WriteDragData(View* sender,
                             int press_x,
                             int press_y,
                             OSExchangeData* data);
  virtual int GetDragOperations(View* sender, int x, int y);

  // Set up the various Views in the toolbar
  void CreateLeftSideControls();
  void CreateCenterStack(Profile* profile);
  void CreateRightSideControls(Profile* profile);

  // Updates the controls to display the security appropriately (highlighted if
  // secure).
  void SetSecurityLevel(ToolbarModel::SecurityLevel security_level);

  // Show the page menu.
  void RunPageMenu(const CPoint& pt, HWND hwnd);

  // Show the app menu.
  void RunAppMenu(const CPoint& pt, HWND hwnd);

  // Overridden from View, to pass keyboard triggering of the right-click
  // context menu on to the toolbar child view that currently has the
  // accessibility focus.
  virtual void ShowContextMenu(int x, int y, bool is_mouse_gesture);

  // Types of display mode this toolbar can have.
  enum DisplayMode {
    DISPLAYMODE_NORMAL,
    DISPLAYMODE_LOCATION
  };
  bool IsDisplayModeNormal() const {
    return display_mode_ == DISPLAYMODE_NORMAL;
  }

  // This View's Command Controller
  CommandController* controller_;

  scoped_ptr<BackForwardMenuModel> back_menu_model_;
  scoped_ptr<BackForwardMenuModel> forward_menu_model_;

  // The model that contains the security level, text, icon to display...
  ToolbarModel* model_;

  // Storage of strings needed for accessibility.
  std::wstring accessible_name_;
  // Child view currently having MSAA focus (location bar excluded from arrow
  // navigation).
  views::View* acc_focused_view_;

  // Controls
  views::Button* back_;
  views::Button* forward_;
  views::Button* reload_;
  views::Button* home_;
  ToolbarStarToggle* star_;
  LocationBarView* location_bar_;
  GoButton* go_;
  views::MenuButton* page_menu_;
  views::MenuButton* app_menu_;
  Profile* profile_;
  Browser* browser_;

  // Current tab we're showing state for.
  TabContents* tab_;

  // Profiles menu to populate with profile names.
  Menu* profiles_menu_;

  // Helper class to enumerate profiles information on the file thread.
  scoped_refptr<GetProfilesHelper> profiles_helper_;

  // Controls whether or not a home button should be shown on the toolbar.
  BooleanPrefMember show_home_button_;

  // The display mode used when laying out the toolbar.
  DisplayMode display_mode_;
};

#endif  // CHROME_BROWSER_VIEWS_TOOLBAR_VIEW_H__
