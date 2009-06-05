// Copyright (c) 2008 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_BLOCKED_POPUP_CONTAINER_VIEW_H_
#define CHROME_BROWSER_VIEWS_BLOCKED_POPUP_CONTAINER_VIEW_H_

#include <set>
#include <utility>
#include <vector>

#include "app/animation.h"
#include "base/gfx/native_widget_types.h"
#include "base/gfx/rect.h"
#include "chrome/browser/blocked_popup_container.h"
#include "chrome/browser/tab_contents/tab_contents_delegate.h"
#include "views/controls/button/button.h"
#include "views/controls/button/menu_button.h"
#include "views/controls/menu/menu.h"
#include "views/view.h"
#include "views/widget/widget_win.h"

class BlockedPopupContainerImpl;
class PrefService;
class Profile;
class TabContents;
class TextButton;

namespace views {
class ImageButton;
}

// The view presented to the user notifying them of the number of popups
// blocked. This view should only be used inside of BlockedPopupContainer.
class BlockedPopupContainerView : public views::View,
                                  public views::ButtonListener,
                                  public views::Menu::Delegate {
 public:
  explicit BlockedPopupContainerView(BlockedPopupContainerImpl* container);
  ~BlockedPopupContainerView();

  // Sets the label on the menu button
  void UpdateLabel();

  std::wstring label() const { return popup_count_label_->text(); }

  // Overridden from views::View:

  // Paints our border and background. (Does not paint children.)
  virtual void Paint(gfx::Canvas* canvas);
  // Sets positions of all child views.
  virtual void Layout();
  // Gets the desired size of the popup notification.
  virtual gfx::Size GetPreferredSize();

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender);

  // Overridden from Menu::Delegate:

  // Displays the status of the "Show Blocked Popup Notification" item.
  virtual bool IsItemChecked(int id) const;
  // Called after user clicks a menu item.
  virtual void ExecuteCommand(int id);

 private:
  // Our owner and HWND parent.
  BlockedPopupContainerImpl* container_;

  // The button which brings up the popup menu.
  views::MenuButton* popup_count_label_;

  // Our "X" button.
  views::ImageButton* close_button_;

  // Popup menu shown to user.
  scoped_ptr<views::Menu> launch_menu_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(BlockedPopupContainerView);
};

// Takes ownership of TabContents that are unrequested popup windows and
// presents an interface to the user for launching them. (Or never showing them
// again).
//
// TODO(erg): When this class goes away, whatever is replaced shouldn't
// multiply inherit.
class BlockedPopupContainerImpl : public BlockedPopupContainer,
                                  public Animation,
                                  public views::WidgetWin {
 public:
  virtual ~BlockedPopupContainerImpl();

  // Returns the URL and title for popup |index|, used to construct a string for
  // display.
  void GetURLAndTitleForPopup(size_t index,
                              std::wstring* url,
                              std::wstring* title) const;

  // Returns the names of hosts showing popups.
  std::vector<std::wstring> GetHosts() const;

  virtual void Destroy();

  virtual void RepositionBlockedPopupContainer(gfx::NativeView view);

 private:
  friend class BlockedPopupContainer;

  // Creates a container for a certain TabContents.
  BlockedPopupContainerImpl(TabContents* owner, PrefService* prefs);

  // Repositions our blocked popup notification so that the lower right corner
  // is at |anchor_point|.
  void RepositionWindowTo(const gfx::Point& anchor_point);

  // Overridden from Animation:
  // Changes the visibility percentage of the BlockedPopupContainerImpl. This is
  // called while animating in or out.
  virtual void AnimateToState(double state);

  // Overridden from views::WidgetWin:

  // Alerts our |owner_| that we are closing ourselves. Cleans up any remaining
  // blocked popups.
  virtual void OnFinalMessage(HWND window);

  // Makes the top corners of the window rounded during resizing events.
  virtual void OnSize(UINT param, const CSize& size);

  // Initializes our Views and positions us to the lower right corner of the
  // browser window.
  void Init(const gfx::Point& initial_anchor);

  // Shows the UI.
  virtual void ShowSelf();

  // Hides the UI portion of the container.
  virtual void HideSelf();

  virtual void UpdateLabel();

  // Sets our position, based on our |anchor_point_| and on our
  // |visibility_percentage_|. This method is called whenever either of those
  // change.
  void SetPosition();

  // Our associated view object.
  BlockedPopupContainerView* container_view_;

  // True while animating in; false while animating out.
  bool in_show_animation_;

  // Percentage of the window to show; used to animate in the notification.
  double visibility_percentage_;

  // The bounds to report to the automation system (may not equal our actual
  // bounds while animating in or out).
  gfx::Rect bounds_;

  // The bottom right corner of where we should appear in our parent window.
  gfx::Point anchor_point_;
};

#endif
