// Copyright (c) 2008 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

// Defines the public interface for the blocked popup notifications. This
// interface should only be used by TabContents. Users and subclasses of
// TabContents should use the appropriate methods on TabContents to access
// information about blocked popups.

#ifndef CHROME_BROWSER_VIEWS_BLOCKED_POPUP_CONTAINER_H_
#define CHROME_BROWSER_VIEWS_BLOCKED_POPUP_CONTAINER_H_

#include <utility>
#include <vector>

#include "base/gfx/rect.h"
#include "chrome/browser/tab_contents/constrained_window.h"
#include "chrome/browser/tab_contents/tab_contents_delegate.h"
#include "chrome/common/animation.h"
#include "chrome/common/pref_member.h"
#include "chrome/views/base_button.h"
#include "chrome/views/menu.h"
#include "chrome/views/view.h"
#include "chrome/views/widget_win.h"

class BlockedPopupContainer;
class Profile;
class TabContents;
class TextButton;

namespace views {
class Button;
class Menu;
class MenuButton;
}

// The view presented to the user notifying them of the number of popups
// blocked. This view should only be used inside of BlockedPopupContainer.
class BlockedPopupContainerView : public views::View,
                                  public views::BaseButton::ButtonListener,
                                  public Menu::Delegate {
 public:
  explicit BlockedPopupContainerView(BlockedPopupContainer* container);
  ~BlockedPopupContainerView();

  // Sets the label on the menu button
  void UpdatePopupCountLabel();

  // Overridden from views::View:

  // Paints our border and background. (Does not paint children.)
  virtual void Paint(ChromeCanvas* canvas);
  // Sets positions of all child views.
  virtual void Layout();
  // Gets the desired size of the popup notification.
  virtual gfx::Size GetPreferredSize();

  // Overridden from views::ButtonListener::ButtonPressed:

  // Called when either the menu button or close button is pressed.
  virtual void ButtonPressed(views::BaseButton* sender);

  // Overridden from Menu::Delegate:

  // Displays the status of the "Show Blocked Popup Notification" item.
  virtual bool IsItemChecked(int id) const;
  // Called after user clicks a menu item.
  virtual void ExecuteCommand(int id);

 private:
  // Our owner and HWND parent.
  BlockedPopupContainer* container_;

  // The button which brings up the popup menu.
  views::MenuButton* popup_count_label_;

  // Our "X" button.
  views::Button* close_button_;

  // Popup menu shown to user.
  scoped_ptr<Menu> launch_menu_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(BlockedPopupContainerView);
};

// Takes ownership of TabContents that are unrequested popup windows and
// presents an interface to the user for launching them. (Or never showing them
// again).
class BlockedPopupContainer : public ConstrainedWindow,
                              public TabContentsDelegate,
                              public views::WidgetWin,
                              public Animation {
 public:
  virtual ~BlockedPopupContainer();

  // Creates a BlockedPopupContainer, anchoring the container to the lower
  // right corner.
  static BlockedPopupContainer* Create(
      TabContents* owner, Profile* profile, const gfx::Point& initial_anchor);

  // Toggles the preference to display this notification.
  void ToggleBlockedPopupNotification();

  // Gets the current state of the show blocked popup notification preference.
  bool GetShowBlockedPopupNotification();

  // Adds a Tabbed contents to this container. |bounds| are the window bounds
  // requested by the popup window.
  void AddTabContents(TabContents* blocked_contents, const gfx::Rect& bounds);

  // Creates a window from blocked popup |index|.
  void LaunchPopupIndex(int index);

  // Returns the number of blocked popups
  int GetTabContentsCount() const;

  // Returns the string to display to the user in the menu for item |index|.
  std::wstring GetDisplayStringForItem(int index);

  // Deletes all popups and hides the interface parts.
  void CloseAllPopups();

  // Called to force this container to never show itself again.
  void set_dismissed() { has_been_dismissed_ = true; }

  // Overridden from ConstrainedWindow:

  // Closes all of our blocked popups and then closes the BlockedPopupContainer.
  virtual void CloseConstrainedWindow();

  // Repositions our blocked popup notification so that the lower right corner
  // is at |anchor_point|.
  virtual void RepositionConstrainedWindowTo(const gfx::Point& anchor_point);

  // A BlockedPopupContainer is part of the HWND heiarchy and therefore doesn't
  // need to manually respond to hide and show events.
  virtual void WasHidden() { }
  virtual void DidBecomeSelected() { }

  // Debugging accessors only called from the unit tests.
  virtual std::wstring GetWindowTitle() const;
  virtual const gfx::Rect& GetCurrentBounds() const;

  // Overridden from TabContentsDelegate:

  // Forwards OpenURLFromTab to our |owner_|.
  virtual void OpenURLFromTab(TabContents* source,
                              const GURL& url, const GURL& referrer,
                              WindowOpenDisposition disposition,
                              PageTransition::Type transition);

  // Ignored; BlockedPopupContainer doesn't display a throbber.
  virtual void NavigationStateChanged(const TabContents* source,
                                      unsigned changed_flags) { }

  // Replaces |source| with |new_contents| in our list of blocked popups,
  // preserving the associated window size.
  virtual void ReplaceContents(TabContents* source,
                               TabContents* new_contents);

  // Forwards AddNewContents to our |owner_|.
  virtual void AddNewContents(TabContents* source,
                              TabContents* new_contents,
                              WindowOpenDisposition disposition,
                              const gfx::Rect& initial_position,
                              bool user_gesture);

  // Ignore activation requests from the TabContents we're blocking.
  virtual void ActivateContents(TabContents* contents) { }

  // Ignored; BlockedPopupContainer doesn't display a throbber.
  virtual void LoadingStateChanged(TabContents* source) { }

  // Removes |source| from our internal list of blocked popups.
  virtual void CloseContents(TabContents* source);

  // Changes the opening rectangle associated with |source|.
  virtual void MoveContents(TabContents* source, const gfx::Rect& pos);

  // Always returns true.
  virtual bool IsPopup(TabContents* source);

  // Returns our |owner_|.
  virtual TabContents* GetConstrainingContents(TabContents* source);

  // Ignored; BlockedPopupContainer doesn't display a toolbar.
  virtual void ToolbarSizeChanged(TabContents* source, bool is_animating) { }

  // Ignored; BlockedPopupContainer doesn't display a bookmarking star.
  virtual void URLStarredChanged(TabContents* source, bool starred) { }

  // Ignored; BlockedPopupContainer doesn't display a URL bar.
  virtual void UpdateTargetURL(TabContents* source, const GURL& url) { }

  // Overridden from Animation:

  // Changes the visibility percentage of the BlockedPopupContainer. This is
  // called while animating in or out.
  virtual void AnimateToState(double state);

 protected:
  // Overridden from views::ContainerWin:

  // Alerts our |owner_| that we are closing ourselves. Cleans up any remaining
  // blocked popups.
  virtual void OnFinalMessage(HWND window);

  // Makes the top corners of the window rounded during resizing events.
  virtual void OnSize(UINT param, const CSize& size);

 private:
  // Creates a container for a certain TabContents.
  BlockedPopupContainer(TabContents* owner, Profile* profile);

  // Initializes our Views and positions us to the lower right corner of the
  // browser window.
  void Init(const gfx::Point& initial_anchor);

  // Hides the UI portion of the container.
  void HideSelf();

  // Shows the UI portion of the container.
  void ShowSelf();

  // Sets our position, based on our |anchor_point_| and on our
  // |visibility_percentage_|. This method is called whenever either of those
  // change.
  void SetPosition();

  // Sends a CloseContents() to each message in |blocked_popups_|.
  void CloseEachTabContents();

  // The TabContents that owns and constrains this BlockedPopupContainer.
  TabContents* owner_;

  // The TabContents and initial positions of all blocked popups.
  std::vector<std::pair<TabContents*, gfx::Rect> > blocked_popups_;

  // Our associated view object.
  BlockedPopupContainerView* container_view_;

  // Link to the block popups preference. Used to both determine whether we
  // should show ourself to the user and to toggle whether we should show this
  // notification to the user.
  BooleanPrefMember block_popup_pref_;

  // Once the container is hidden, this is set to prevent it from reappearing.
  bool has_been_dismissed_;

  // True while animating in; false while animating out.
  bool in_show_animation_;

  // Percentage of the window to show; used to animate in the notification.
  double visibility_percentage_;

  // The bounds to report to the automation system (may not equal our actual
  // bounds while animating in or out).
  gfx::Rect bounds_;

  // The bottom right corner of where we should appear in our parent window.
  gfx::Point anchor_point_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(BlockedPopupContainer);
};

#endif
