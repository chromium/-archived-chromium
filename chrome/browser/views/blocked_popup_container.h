// Copyright (c) 2008 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_BLOCKED_POPUP_CONTAINER_H_
#define CHROME_BROWSER_VIEWS_BLOCKED_POPUP_CONTAINER_H_

#include <vector>

#include "base/gfx/rect.h"
#include "chrome/browser/constrained_window.h"
#include "chrome/browser/tab_contents_delegate.h"
#include "chrome/common/animation.h"
#include "chrome/common/pref_member.h"
#include "chrome/views/container_win.h"

class BlockedPopupContainerView;
class Profile;
class TabContents;
class TextButton;

///////////////////////////////////////////////////////////////////////////////
//
// BlockedPopupContainer
//
//  This class takes ownership of TabContents that are unrequested popup
//  windows and presents an interface to the user for launching them. (Or never
//  showing them again)
//
class BlockedPopupContainer : public ConstrainedWindow,
                              public TabContentsDelegate,
                              public views::ContainerWin,
                              public Animation {
 public:
  virtual ~BlockedPopupContainer();

  // Create a BlockedPopupContainer, anchoring the container to the lower right
  // corner.
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

  // Return the number of blocked popups
  int GetTabContentsCount() const;

  // Returns the string to display to the user in the menu for item |index|.
  std::wstring GetDisplayStringForItem(int index);

  // Deletes all popups and hides the interface parts.
  void CloseAllPopups();

  // Called to force this container to never show itself again.
  void set_dismissed() { has_been_dismissed_ = true; }

  // Override from ConstrainedWindow:
  virtual void CloseConstrainedWindow();
  virtual void RepositionConstrainedWindowTo(const gfx::Point& anchor_point);
  virtual void WasHidden() { }
  virtual void DidBecomeSelected() { }
  virtual std::wstring GetWindowTitle() const;
  virtual const gfx::Rect& GetCurrentBounds() const;

  // Override from TabContentsDelegate:
  virtual void OpenURLFromTab(TabContents* source,
                              const GURL& url, const GURL& referrer,
                              WindowOpenDisposition disposition,
                              PageTransition::Type transition);
  virtual void NavigationStateChanged(const TabContents* source,
                                      unsigned changed_flags) { }
  virtual void ReplaceContents(TabContents* source,
                               TabContents* new_contents);
  virtual void AddNewContents(TabContents* source,
                              TabContents* new_contents,
                              WindowOpenDisposition disposition,
                              const gfx::Rect& initial_pos,
                              bool user_gesture);
  virtual void ActivateContents(TabContents* contents) { }
  virtual void LoadingStateChanged(TabContents* source) { }
  virtual void CloseContents(TabContents* source);
  virtual void MoveContents(TabContents* source, const gfx::Rect& pos);
  virtual bool IsPopup(TabContents* source);
  virtual TabContents* GetConstrainingContents(TabContents* source);
  virtual void ToolbarSizeChanged(TabContents* source, bool is_animating) { }
  virtual void URLStarredChanged(TabContents* source, bool starred) { }
  virtual void UpdateTargetURL(TabContents* source, const GURL& url) { }

  // Override from Animation:
  virtual void AnimateToState(double state);

 protected:
  // Override from views::ContainerWin:
  virtual void OnFinalMessage(HWND window);
  virtual void OnSize(UINT param, const CSize& size);

 private:
  // Create a container for a certain TabContents.
  BlockedPopupContainer(TabContents* owner, Profile* profile);

  // Initialize our Views and positions us to the lower right corner of the
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

  // Send a CloseContents() to each message in |blocked_popups_|.
  void CloseEachTabContents();

  // The TabContents that owns and constrains this BlockedPopupContainer.
  TabContents* owner_;

  // TabContents.
  std::vector<std::pair<TabContents*, gfx::Rect> > blocked_popups_;

  // Our associated view object.
  BlockedPopupContainerView* container_view_;

  // Link to the show blocked popup preference. Used to both determine whether
  // we should show ourself to the user...
  BooleanPrefMember block_popup_pref_;

  // Once the container is hidden, this is set to prevent it from reappearing.
  bool has_been_dismissed_;

  // True while animation in; false while animating out.
  bool in_show_animation_;

  // Percentage of the window to show; used to animate in the notification.
  double visibility_percentage_;

  // The bounds to report to the automation system (may not equal our actual
  // bounds while animating in or out).
  gfx::Rect bounds_;

  // The bottom right corner of where we should appear in our parent window.
  gfx::Point anchor_point_;

  DISALLOW_COPY_AND_ASSIGN(BlockedPopupContainer);
};

#endif
