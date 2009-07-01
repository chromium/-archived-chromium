// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

// Defines the public interface for the blocked popup notifications. This
// interface should only be used by TabContents. Users and subclasses of
// TabContents should use the appropriate methods on TabContents to access
// information about blocked popups.

#ifndef CHROME_BROWSER_BLOCKED_POPUP_CONTAINER_H_
#define CHROME_BROWSER_BLOCKED_POPUP_CONTAINER_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/gfx/native_widget_types.h"
#include "base/gfx/rect.h"
#include "chrome/browser/tab_contents/constrained_window.h"
#include "chrome/browser/tab_contents/tab_contents_delegate.h"
#include "chrome/common/notification_registrar.h"

class BlockedPopupContainer;
class PrefService;
class Profile;
class TabContents;

// Interface that the BlockedPopupContainer model/controller uses to
// communicate with its platform specific view.
class BlockedPopupContainerView {
 public:
  // Platform specific constructor used by BlockedPopupContainer to create a
  // view that is displayed to the user.
  static BlockedPopupContainerView* Create(BlockedPopupContainer* container);

  // Notification that the view should properly position itself in |view|.
  virtual void SetPosition() = 0;

  // Shows the blocked popup view / Animates the blocked popup view in.
  virtual void ShowView() = 0;

  // Sets the text in the blocked popup label.
  virtual void UpdateLabel() = 0;

  // Hides the blocked popup view / Animates the blocked popup view out.
  virtual void HideView() = 0;

  // Called by the BlockedPopupContainer that owns us. Destroys the view or
  // starts a delayed Task to destroy the View at some later time.
  virtual void Destroy() = 0;
};

// Takes ownership of TabContents that are unrequested popup windows and
// presents an interface to the user for launching them. (Or never showing them
// again). This class contains all the cross-platform bits that can be used in
// all ports.
//
// TODO(erg): The GTK and Cocoa versions of the view class haven't been written
// yet.
//
// +- BlockedPopupContainer ---+         +- BlockedPopupContainerView -----+
// | All model logic           |    +--->| Abstract cross platform         |
// |                           |    |    | interface                       |
// |                           |    |    |                                 |
// | Owns a platform view_     +----+    |                                 |
// +---------------------------+         +---------------------------------+
//                                                  ^
//                                                  |
//                  +-------------------------------+-----------+
//                  |                                           |
//  +- BPCViewGtk -----------+     +- BPCViewWin ----------------------+
//  | Gtk UI                 |     | Views UI                          |
//  |                        |     |                                   |
//  +------------------------+     +-----------------------------------+
//
// Getting here will take multiple patches.
class BlockedPopupContainer : public TabContentsDelegate,
                              public NotificationObserver {
 public:
  // Creates a BlockedPopupContainer, anchoring the container to the lower
  // right corner.
  static BlockedPopupContainer* Create(TabContents* owner, Profile* profile);

  static void RegisterUserPrefs(PrefService* prefs);

  // Sets this BlockedPopupContainer's view. BlockedPopupContainer now owns
  // |view| and is responsible for calling Destroy() on it.
  void set_view(BlockedPopupContainerView* view) { view_ = view; }

  // Adds a popup to this container. |bounds| are the window bounds requested by
  // the popup window.
  void AddTabContents(TabContents* tab_contents,
                      const gfx::Rect& bounds,
                      const std::string& host);

  // Shows the blocked popup at index |index|.
  void LaunchPopupAtIndex(size_t index);

  // Returns the number of blocked popups
  size_t GetBlockedPopupCount() const;

  // Returns true if host |index| is whitelisted.  Returns false if |index| is
  // invalid.
  bool IsHostWhitelisted(size_t index) const;

  // If host |index| is currently whitelisted, un-whitelists it.  Otherwise,
  // whitelists it and opens all blocked popups from it.
  void ToggleWhitelistingForHost(size_t index);

  // Deletes all popups and hides the interface parts.
  void CloseAll();

  // Sets this object up to delete itself.
  void Destroy();

  // Message called when a BlockedPopupContainer should move itself to the
  // bottom right corner of our parent view.
  void RepositionBlockedPopupContainer();

  // Returns the TabContents for the blocked popup |index|.
  TabContents* GetTabContentsAt(size_t index) const;

  // Returns the names of hosts showing popups.
  std::vector<std::string> GetHosts() const;

  // Deletes all local state.
  void ClearData();

  // Called to force this container to never show itself again.
  void set_dismissed() { has_been_dismissed_ = true; }

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

  // A number larger than the internal popup count on the Renderer; meant for
  // preventing a compromised renderer from exhausting GDI memory by spawning
  // infinite windows.
  static const size_t kImpossibleNumberOfPopups = 30;

 protected:
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

  // Hides the UI portion of the container.
  void HideSelf();

  // Helper function to convert a host index (which the view uses) into an
  // iterator into |popup_hosts_|.  Returns popup_hosts_.end() if |index| is
  // invalid.
  PopupHosts::const_iterator ConvertHostIndexToIterator(size_t index) const;

  // Removes the popup at |i| from the blocked popup list.  If this popup's host
  // is not otherwised referenced on either popup list, removes the host from
  // the host list.  Updates the view's label to match the new state.
  void EraseDataForPopupAndUpdateUI(BlockedPopups::iterator i);

  // Same as above, but works on the unblocked popup list.
  void EraseDataForPopupAndUpdateUI(UnblockedPopups::iterator i);

 private:
  friend class BlockedPopupContainerImpl;
  friend class BlockedPopupContainerTest;

  // string is hostname.
  typedef std::set<std::string> Whitelist;

  // Creates a container for a certain TabContents:
  BlockedPopupContainer(TabContents* owner, PrefService* prefs);

  // Either hides the view if there are no popups, or updates the label if
  // there are.
  void UpdateView();

  // Overridden from notificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // The TabContents that owns and constrains this BlockedPopupContainer.
  TabContents* owner_;

  // The PrefService we can query to find out what's on the whitelist.
  PrefService* prefs_;

  // Once the container is hidden, this is set to prevent it from reappearing.
  bool has_been_dismissed_;

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

  // Our platform specific view.
  BlockedPopupContainerView* view_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(BlockedPopupContainer);
};

#endif  // CHROME_BROWSER_BLOCKED_POPUP_CONTAINER_H_
