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

#ifndef CHROME_BROWSER_VIEWS_TOOLBAR_VIEW_H__
#define CHROME_BROWSER_VIEWS_TOOLBAR_VIEW_H__

#include <vector>

#include "base/scoped_ptr.h"
#include "chrome/browser/back_forward_menu_model.h"
#include "chrome/browser/controller.h"
#include "chrome/browser/encoding_menu_controller_delegate.h"
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
class BrowserToolbarView : public ChromeViews::View,
                           public EncodingMenuControllerDelegate,
                           public ChromeViews::ViewMenuDelegate,
                           public ChromeViews::DragController,
                           public LocationBarView::Delegate,
                           public NotificationObserver {
 public:
  BrowserToolbarView(CommandController* controller, Browser* browser);
  virtual ~BrowserToolbarView();

  // Create the contents of the Browser Toolbar
  void Init(Profile* profile);

  // ChromeViews::View
  virtual void Layout();
  virtual void DidGainFocus();
  virtual void WillLoseFocus();
  virtual bool OnKeyPressed(const ChromeViews::KeyEvent& e);
  virtual bool OnKeyReleased(const ChromeViews::KeyEvent& e);

  // Overridden from EncodingMenuControllerDelegate:
  virtual bool IsItemChecked(int id) const;

  // Overridden from Menu::BaseControllerDelegate:
  virtual bool GetAcceleratorInfo(int id, ChromeViews::Accelerator* accel);

  // ChromeViews::MenuDelegate
  virtual void RunMenu(ChromeViews::View* source, const CPoint& pt, HWND hwnd);

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

  ChromeViews::View* GetAccFocusedChildView() {
    return acc_focused_view_;
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
  ChromeViews::View* acc_focused_view_;

  // Controls
  ChromeViews::Button* back_;
  ChromeViews::Button* forward_;
  ChromeViews::Button* reload_;
  ChromeViews::Button* home_;
  ToolbarStarToggle* star_;
  LocationBarView* location_bar_;
  GoButton* go_;
  ChromeViews::MenuButton* page_menu_;
  ChromeViews::MenuButton* app_menu_;
  Profile* profile_;
  Browser* browser_;

  // Current tab we're showing state for.
  TabContents* tab_;

  // Controls whether or not a home button should be shown on the toolbar.
  BooleanPrefMember show_home_button_;
};

#endif  // CHROME_BROWSER_VIEWS_TOOLBAR_VIEW_H__
