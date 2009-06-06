// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_TOOLBAR_VIEW_H_
#define CHROME_BROWSER_VIEWS_TOOLBAR_VIEW_H_

#include <vector>

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/encoding_menu_controller_delegate.h"
#include "chrome/browser/user_data_manager.h"
#include "chrome/browser/views/autocomplete/autocomplete_popup_contents_view.h"
#include "chrome/browser/views/go_button.h"
#include "chrome/browser/views/location_bar_view.h"
#include "chrome/common/pref_member.h"
#include "views/controls/button/menu_button.h"
#include "views/controls/menu/menu.h"
#include "views/controls/menu/view_menu_delegate.h"
#include "views/view.h"

class BackForwardMenuModelViews;
class Browser;
class Profile;
class ToolbarStarToggle;

// The Browser Window's toolbar. Used within BrowserView.
class ToolbarView : public views::View,
                    public EncodingMenuControllerDelegate,
                    public views::ViewMenuDelegate,
                    public views::DragController,
                    public LocationBarView::Delegate,
                    public NotificationObserver,
                    public GetProfilesHelper::Delegate,
                    public CommandUpdater::CommandObserver,
                    public views::ButtonListener,
                    public AutocompletePopupPositioner {
 public:
  explicit ToolbarView(Browser* browser);
  virtual ~ToolbarView();

  // Create the contents of the Browser Toolbar
  void Init(Profile* profile);

  // Sets the profile which is active on the currently-active tab.
  void SetProfile(Profile* profile);
  Profile* profile() { return profile_; }

  // Updates the toolbar (and transitively the location bar) with the states of
  // the specified |tab|.  If |should_restore_state| is true, we're switching
  // (back?) to this tab and should restore any previous location bar state
  // (such as user editing) as well.
  void Update(TabContents* tab, bool should_restore_state);

  // Returns the index of the next view of the toolbar, starting from the given
  // view index (skipping the location bar), in the given navigation direction
  // (nav_left true means navigation right to left, and vice versa). -1 finds
  // first accessible child, based on the above policy.
  int GetNextAccessibleViewIndex(int view_index, bool nav_left);

  void set_acc_focused_view(views::View* acc_focused_view) {
    acc_focused_view_ = acc_focused_view;
  }

  // Accessors...
  Browser* browser() const { return browser_; }
  ToolbarStarToggle* star_button() const { return star_; }
  GoButton* go_button() const { return go_; }
  LocationBarView* location_bar() const { return location_bar_; }

  // Overridden from EncodingMenuControllerDelegate:
  virtual bool IsItemChecked(int id) const;

  // Overridden from Menu::BaseControllerDelegate:
  virtual bool GetAcceleratorInfo(int id, views::Accelerator* accel);

  // Overridden from views::MenuDelegate:
  virtual void RunMenu(views::View* source, const gfx::Point& pt,
                       gfx::NativeView hwnd);

  // Overridden from GetProfilesHelper::Delegate:
  virtual void OnGetProfilesDone(const std::vector<std::wstring>& profiles);

  // Overridden from LocationBarView::Delegate:
  virtual TabContents* GetTabContents();
  virtual void OnInputInProgress(bool in_progress);

  // Overridden from CommandUpdater::CommandObserver:
  virtual void EnabledStateChangedForCommand(int id, bool enabled);

  // Overridden from views::BaseButton::ButtonListener:
  virtual void ButtonPressed(views::Button* sender);

  // Overridden from AutocompletePopupPositioner:
  virtual gfx::Rect GetPopupBounds() const;

  // Overridden from NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize();
  virtual void Layout();
  virtual void Paint(gfx::Canvas* canvas);
  virtual void ThemeChanged();
  virtual void ShowContextMenu(int x, int y, bool is_mouse_gesture);
  virtual void DidGainFocus();
  virtual void WillLoseFocus();
  virtual bool OnKeyPressed(const views::KeyEvent& e);
  virtual bool OnKeyReleased(const views::KeyEvent& e);
  virtual bool GetAccessibleName(std::wstring* name);
  virtual bool GetAccessibleRole(AccessibilityTypes::Role* role);
  virtual void SetAccessibleName(const std::wstring& name);
  virtual View* GetAccFocusedChildView() { return acc_focused_view_; }

 private:
  // Returns the number of pixels above the location bar in non-normal display.
  int PopupTopSpacing() const;

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
  void LoadLeftSideControlsImages();
  void LoadCenterStackImages();
  void LoadRightSideControlsImages();

  // Runs various menus.
  void RunPageMenu(const gfx::Point& pt, gfx::NativeView hwnd);
  void RunAppMenu(const gfx::Point& pt, gfx::NativeView hwnd);

  // Types of display mode this toolbar can have.
  enum DisplayMode {
    DISPLAYMODE_NORMAL,       // Normal toolbar with buttons, etc.
    DISPLAYMODE_LOCATION      // Slimline toolbar showing only compact location
                              // bar, used for popups.
  };
  bool IsDisplayModeNormal() const {
    return display_mode_ == DISPLAYMODE_NORMAL;
  }

  scoped_ptr<BackForwardMenuModelViews> back_menu_model_;
  scoped_ptr<BackForwardMenuModelViews> forward_menu_model_;

  // The model that contains the security level, text, icon to display...
  ToolbarModel* model_;

  // Storage of strings needed for accessibility.
  std::wstring accessible_name_;
  // Child view currently having MSAA focus (location bar excluded from arrow
  // navigation).
  views::View* acc_focused_view_;

  // Controls
  views::ImageButton* back_;
  views::ImageButton* forward_;
  views::ImageButton* reload_;
  views::ImageButton* home_;
  ToolbarStarToggle* star_;
  LocationBarView* location_bar_;
  GoButton* go_;
  views::MenuButton* page_menu_;
  views::MenuButton* app_menu_;
  // The bookmark menu button. This may be null.
  views::MenuButton* bookmark_menu_;
  Profile* profile_;
  Browser* browser_;

  // Current tab we're showing state for.
  TabContents* tab_;

  // Profiles menu to populate with profile names.
  views::Menu* profiles_menu_;

  // Helper class to enumerate profiles information on the file thread.
  scoped_refptr<GetProfilesHelper> profiles_helper_;

  // Controls whether or not a home button should be shown on the toolbar.
  BooleanPrefMember show_home_button_;

  // The display mode used when laying out the toolbar.
  DisplayMode display_mode_;
};

#endif  // CHROME_BROWSER_VIEWS_TOOLBAR_VIEW_H_
