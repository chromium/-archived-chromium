// Copyright (c) 2008 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

// Defines the public interface for the blocked popup notifications. This
// interface should only be used by TabContents. Users and subclasses of
// TabContents should use the appropriate methods on TabContents to access
// information about blocked popups.

#ifndef CHROME_BROWSER_VIEWS_BLOCKED_POPUP_CONTAINER_H_
#define CHROME_BROWSER_VIEWS_BLOCKED_POPUP_CONTAINER_H_

#include <set>
#include <utility>
#include <vector>

#include "app/animation.h"
#include "base/gfx/rect.h"
#include "chrome/browser/tab_contents/constrained_window.h"
#include "chrome/browser/tab_contents/tab_contents_delegate.h"
#include "chrome/common/notification_registrar.h"
#include "views/controls/button/button.h"
#include "views/controls/button/menu_button.h"
#include "views/controls/menu/menu.h"
#include "views/view.h"
#include "views/widget/widget_win.h"

class BlockedPopupContainer;
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
  explicit BlockedPopupContainerView(BlockedPopupContainer* container);
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
  BlockedPopupContainer* container_;

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
class BlockedPopupContainer : public Animation,
                              public ConstrainedWindow,
                              public NotificationObserver,
                              public TabContentsDelegate,
                              public views::WidgetWin {
 public:
  virtual ~BlockedPopupContainer();

  static void RegisterUserPrefs(PrefService* prefs);

  // Creates a BlockedPopupContainer, anchoring the container to the lower
  // right corner.
  static BlockedPopupContainer* Create(
      TabContents* owner, Profile* profile, const gfx::Point& initial_anchor);

  // Adds a popup to this container. |bounds| are the window bounds requested by
  // the popup window.
  void AddTabContents(TabContents* tab_contents,
                      const gfx::Rect& bounds,
                      const std::string& host);

  // Shows the blocked popup at index |index|.
  void LaunchPopupAtIndex(size_t index);

  // Returns the number of blocked popups
  size_t GetBlockedPopupCount() const;

  // Returns the URL and title for popup |index|, used to construct a string for
  // display.
  void GetURLAndTitleForPopup(size_t index,
                              std::wstring* url,
                              std::wstring* title) const;

  // Returns the names of hosts showing popups.
  std::vector<std::wstring> GetHosts() const;

  // Returns true if host |index| is whitelisted.  Returns false if |index| is
  // invalid.
  bool IsHostWhitelisted(size_t index) const;

  // If host |index| is currently whitelisted, un-whitelists it.  Otherwise,
  // whitelists it and opens all blocked popups from it.
  void ToggleWhitelistingForHost(size_t index);

  // Deletes all popups and hides the interface parts.
  void CloseAll();

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
  virtual std::wstring GetWindowTitle() const {
    return container_view_->label();
  }
  virtual const gfx::Rect& GetCurrentBounds() const { return bounds_; }

  // Overridden from TabContentsDelegate:

  // Forwards OpenURLFromTab to our |owner_|.
  virtual void OpenURLFromTab(TabContents* source,
                              const GURL& url, const GURL& referrer,
                              WindowOpenDisposition disposition,
                              PageTransition::Type transition);

  // Ignored; BlockedPopupContainer doesn't display a throbber.
  virtual void NavigationStateChanged(const TabContents* source,
                                      unsigned changed_flags) { }

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
  virtual void MoveContents(TabContents* source, const gfx::Rect& new_bounds);

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

  // Creates an ExtensionFunctionDispatcher that has no browser
  virtual ExtensionFunctionDispatcher* CreateExtensionFunctionDispatcher(
      RenderViewHost* render_view_host,
      const std::string& extension_id);

 private:
  struct BlockedPopup {
    BlockedPopup(TabContents* tab_contents,
                 const gfx::Rect& bounds,
                 const std::string& host)
        : tab_contents(tab_contents), bounds(bounds), host(host) {
    }

    TabContents* tab_contents;
    gfx::Rect bounds;
    std::string host;
  };
  typedef std::vector<BlockedPopup> BlockedPopups;

  // TabContents is the popup contents.  string is opener hostname.
  typedef std::map<TabContents*, std::string> UnblockedPopups;

  // string is hostname.  bool is whitelisted status.
  typedef std::map<std::string, bool> PopupHosts;

  // string is hostname.
  typedef std::set<std::string> Whitelist;

  // Creates a container for a certain TabContents.
  BlockedPopupContainer(TabContents* owner, PrefService* prefs);

  // Overridden from Animation:
  // Changes the visibility percentage of the BlockedPopupContainer. This is
  // called while animating in or out.
  virtual void AnimateToState(double state);

  // Overridden from notificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Overridden from views::WidgetWin:

  // Alerts our |owner_| that we are closing ourselves. Cleans up any remaining
  // blocked popups.
  virtual void OnFinalMessage(HWND window);

  // Makes the top corners of the window rounded during resizing events.
  virtual void OnSize(UINT param, const CSize& size);

  // Initializes our Views and positions us to the lower right corner of the
  // browser window.
  void Init(const gfx::Point& initial_anchor);

  // Hides the UI portion of the container.
  void HideSelf();

  // Sets our position, based on our |anchor_point_| and on our
  // |visibility_percentage_|. This method is called whenever either of those
  // change.
  void SetPosition();

  // Deletes all local state.
  void ClearData();

  // Helper function to convert a host index (which the view uses) into an
  // iterator into |popup_hosts_|.  Returns popup_hosts_.end() if |index| is
  // invalid.
  PopupHosts::const_iterator ConvertHostIndexToIterator(size_t index) const;

  // Removes the popup at |i| from the blocked popup list.  If this popup's host
  // is not otherwised referenced on either popup list, removes the host from
  // the host list.  Updates the view's label to match the new state.
  void EraseDataForPopupAndUpdateUI(BlockedPopups::iterator i);

  // Same as above, but works on the unblocked popup list, and returns the
  // iterator that results from calling erase().
  UnblockedPopups::iterator EraseDataForPopupAndUpdateUI(
      UnblockedPopups::iterator i);

  // The TabContents that owns and constrains this BlockedPopupContainer.
  TabContents* owner_;

  // The PrefService we can query to find out what's on the whitelist.
  PrefService* prefs_;

  // Registrar to handle notifications we care about.
  NotificationRegistrar registrar_;

  // The whitelisted hosts, which we allow to open popups directly.
  Whitelist whitelist_;

  // Information about all blocked popups.
  BlockedPopups blocked_popups_;

  // Information about all unblocked popups.
  UnblockedPopups unblocked_popups_;

  // Information about all popup hosts.
  PopupHosts popup_hosts_;

  // Our associated view object.
  BlockedPopupContainerView* container_view_;

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
