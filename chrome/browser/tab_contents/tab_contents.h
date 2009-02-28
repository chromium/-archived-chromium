// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_TAB_CONTENTS_H_
#define CHROME_BROWSER_TAB_CONTENTS_TAB_CONTENTS_H_

#include "build/build_config.h"

#include <string>
#include <vector>

#include "base/gfx/native_widget_types.h"
#include "base/gfx/rect.h"
#if defined(OS_WIN)
// TODO(evanm): I mean really, c'mon, this can't have broken the build, right?
#include "chrome/browser/autocomplete/autocomplete_edit.h"
#endif
#include "chrome/browser/tab_contents/constrained_window.h"
#include "chrome/browser/tab_contents/infobar_delegate.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/browser/tab_contents/page_navigator.h"
#include "chrome/browser/tab_contents/tab_contents_type.h"
#include "chrome/common/navigation_types.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/property_bag.h"

namespace gfx {
class Rect;
class Size;
}
namespace views {
class RootView;
class WindowDelegate;
}

class BlockedPopupContainer;
class DOMUIHost;
class DownloadItem;
class DownloadShelf;
class InfoBarView;
class LoadNotificationDetails;
class Profile;
class TabContentsDelegate;
class TabContentsFactory;
class SkBitmap;
class SiteInstance;
class WebContents;

// Describes what goes in the main content area of a tab.  For example,
// the WebContents is one such thing.
//
// When instantiating a new TabContents explicitly, the TabContents will not
// have an associated NavigationController.  To setup a NavigationController
// for the TabContents, its SetupController method should be called.
//
// Once they reside within a NavigationController, TabContents objects are
// owned by that NavigationController. When the active TabContents within that
// NavigationController is closed, that TabContents destroys the
// NavigationController, which then destroys all of the TabContentses in it.
//
// NOTE: When the NavigationController is navigated to an URL corresponding to
// a different type of TabContents (see the TabContents::TypeForURL method),
// the NavigationController makes the active TabContents inactive, notifies the
// TabContentsDelegate that the TabContents is being replaced, and then
// activates the new TabContents.
class TabContents : public PageNavigator,
                    public NotificationObserver {
 public:
  // Flags passed to the TabContentsDelegate.NavigationStateChanged to tell it
  // what has changed. Combine them to update more than one thing.
  enum InvalidateTypes {
    INVALIDATE_URL = 1,      // The URL has changed.
    INVALIDATE_TITLE = 2,    // The title has changed.
    INVALIDATE_FAVICON = 4,  // The favicon has changed.
    INVALIDATE_LOAD = 8,     // The loading state has changed

    // Helper for forcing a refresh.
    INVALIDATE_EVERYTHING = 0xFFFFFFFF
  };

  static void RegisterUserPrefs(PrefService* prefs);

  // Factory -------------------------------------------------------------------
  // (implemented in tab_contents_factory.cc)

  // Creates a new TabContents of the given type.  Will reuse the given
  // instance's renderer, if it is not null.
  static TabContents* CreateWithType(TabContentsType type,
                                     Profile* profile,
                                     SiteInstance* instance);

  // Returns the type of TabContents needed to handle the URL. |url| may
  // end up being modified to contain the _real_ url being loaded if the
  // parameter was an alias (such as about: urls and chrome- urls).
  static TabContentsType TypeForURL(GURL* url);

  // This method can be used to register a new TabContents type dynamically,
  // which can be very useful for unit testing.  If factory is null, then the
  // tab contents type is unregistered.  Returns the previously registered
  // factory for the given type or null if there was none.
  static TabContentsFactory* RegisterFactory(TabContentsType type,
                                             TabContentsFactory* factory);

  // Creation & destruction ----------------------------------------------------

  // Request this tab to shut down. This kills the tab's NavigationController,
  // which then Destroy()s all tabs it controls.
  void CloseContents();

  // Unregister/shut down any pending tasks involving this tab.
  // This is called as the tab is shutting down, before the
  // NavigationController (and consequently profile) are gone.
  //
  // If you override this, be sure to call this implementation at the end
  // of yours.
  // See also Close().
  virtual void Destroy();

  // Intrinsic tab state -------------------------------------------------------

  // Returns the type of tab this is. See also the As* functions following.
  TabContentsType type() const { return type_; }

  // Returns the property bag for this tab contents, where callers can add
  // extra data they may wish to associate with the tab. Returns a pointer
  // rather than a reference since the PropertyAccessors expect this.
  const PropertyBag* property_bag() const { return &property_bag_; }
  PropertyBag* property_bag() { return &property_bag_; }

  // Returns this object as a WebContents if it is one, and NULL otherwise.
  virtual WebContents* AsWebContents() { return NULL; }

  // Const version of above for situations where const TabContents*'s are used.
  WebContents* AsWebContents() const {
    return const_cast<TabContents*>(this)->AsWebContents();
  }

  // Returns this object as a DOMUIHost if it is one, and NULL otherwise.
  virtual DOMUIHost* AsDOMUIHost() { return NULL; }

  TabContentsDelegate* delegate() const { return delegate_; }
  void set_delegate(TabContentsDelegate* d) { delegate_ = d; }

  // This can only be null if the TabContents has been created but
  // SetupController has not been called. The controller should always outlive
  // its TabContents.
  NavigationController* controller() const { return controller_; }
  void set_controller(NavigationController* c) { controller_ = c; }

  // Sets up a new NavigationController for this TabContents.
  // |profile| is the user profile that should be associated with
  // the new controller.
  //
  // TODO(brettw) this seems bogus and I couldn't find any legitimate need for
  // it. I think it should be passed in the constructor.
  void SetupController(Profile* profile);

  // Returns the user profile associated with this TabContents (via the
  // NavigationController).  This will return NULL if there isn't yet a
  // NavigationController on this TabContents.
  // TODO(darin): make it so that controller_ can never be null
  Profile* profile() const {
    return controller_ ? controller_->profile() : NULL;
  }

  // Returns whether this tab contents supports the provided URL. By default,
  // this method matches the tab contents type with the result of TypeForURL().
  // |url| points to the actual URL that will be used. It can be modified as
  // needed.
  // Override this method if your TabContents subclass supports various URL
  // schemes but doesn't want to be the default handler for these schemes.
  // For example, the NewTabUIContents overrides this method to support
  // javascript: URLs.
  virtual bool SupportsURL(GURL* url);

  // Tab navigation state ------------------------------------------------------

  // Returns the current navigation properties, which if a navigation is
  // pending may be provisional (e.g., the navigation could result in a
  // download, in which case the URL would revert to what it was previously).
  const GURL& GetURL() const;
  virtual const std::wstring& GetTitle() const;

  // The max PageID of any page that this TabContents has loaded.  PageIDs
  // increase with each new page that is loaded by a tab.  If this is a
  // WebContents, then the max PageID is kept separately on each SiteInstance.
  // Returns -1 if no PageIDs have yet been seen.
  int32 GetMaxPageID();

  // Updates the max PageID to be at least the given PageID.
  void UpdateMaxPageID(int32 page_id);

  // Returns the site instance associated with the current page. By default,
  // there is no site instance. WebContents overrides this to provide proper
  // access to its site instance.
  virtual SiteInstance* GetSiteInstance() const { return NULL; }

  // Initial title assigned to NavigationEntries from Navigate.
  virtual const std::wstring GetDefaultTitle() const;

  // Defines whether this tab's URL should be displayed in the browser's URL
  // bar. Normally this is true so you can see the URL. This is set to false
  // for the new tab page and related pages so that the URL bar is empty and
  // the user is invited to type into it.
  virtual bool ShouldDisplayURL() { return true; }

  // Returns the favicon for this tab, or an isNull() bitmap if the tab does not
  // have a favicon. The default implementation uses the current navigation
  // entry.
  virtual SkBitmap GetFavIcon() const;

  // Returns whether the favicon should be displayed. If this returns false, no
  // space is provided for the favicon, and the favicon is never displayed.
  virtual bool ShouldDisplayFavIcon() { return true; }

  // SSL related states.
  SecurityStyle GetSecurityStyle() const;

  // Sets |ev_text| to the text that should be displayed in the EV label of
  // the location bar and |ev_tooltip_text| to the tooltip for that label.
  // Returns false and sets these strings to empty if the current page is either
  // not served over HTTPS or if HTTPS does not use an EV cert.
  bool GetSSLEVText(std::wstring* ev_text, std::wstring* ev_tooltip_text) const;

  // Returns a human-readable description the tab's loading state.
  virtual std::wstring GetStatusText() const { return std::wstring(); }

  // Return whether this tab contents is loading a resource.
  bool is_loading() const { return is_loading_; }

  // Returns whether this tab contents is waiting for a first-response for the
  // main resource of the page. This controls whether the throbber state is
  // "waiting" or "loading."
  bool waiting_for_response() const { return waiting_for_response_; }

  // Internal state ------------------------------------------------------------

  // This flag indicates whether the tab contents is currently being
  // screenshotted by the DraggedTabController.
  bool capturing_contents() const { return capturing_contents_; }
  void set_capturing_contents(bool cap) { capturing_contents_ = cap; }

  // Indicates whether this tab should be considered crashed. The setter will
  // also notify the delegate when the flag is changed.
  bool is_crashed() const { return is_crashed_; }
  void SetIsCrashed(bool state);

  // Set whether this tab contents is active. A tab content is active for a
  // given tab if it is currently being used to display some contents. Note that
  // this is different from whether a tab is selected.
  bool is_active() const { return is_active_; }
  void set_is_active(bool active) { is_active_ = active; }

  // Whether the tab is in the process of being destroyed.
  // Added as a tentative work-around for focus related bug #4633.  This allows
  // us not to store focus when a tab is being closed.
  bool is_being_destroyed() const { return is_being_destroyed_; }

  // Convenience method for notifying the delegate of a navigation state
  // change. See TabContentsDelegate.
  void NotifyNavigationStateChanged(unsigned changed_flags);

  // Invoked when the tab contents becomes selected. If you override, be sure
  // and invoke super's implementation.
  virtual void DidBecomeSelected();

  // Invoked when the tab contents becomes hidden.
  // NOTE: If you override this, call the superclass version too!
  virtual void WasHidden();

  // Activates this contents within its containing window, bringing that window
  // to the foreground if necessary.
  virtual void Activate();

  // Commands ------------------------------------------------------------------

  // Implementation of PageNavigator.
  virtual void OpenURL(const GURL& url, const GURL& referrer,
                       WindowOpenDisposition disposition,
                       PageTransition::Type transition);

  // Called by the NavigationController to cause the TabContents to navigate to
  // the current pending entry. The NavigationController should be called back
  // with CommitPendingEntry/RendererDidNavigate on success or
  // DiscardPendingEntry. The callbacks can be inside of this function, or at
  // some future time.
  //
  // The entry has a PageID of -1 if newly created (corresponding to navigation
  // to a new URL).
  //
  // If this method returns false, then the navigation is discarded (equivalent
  // to calling DiscardPendingEntry on the NavigationController).
  virtual bool NavigateToPendingEntry(bool reload);

  // Stop any pending navigation.
  virtual void Stop() {}

  // TODO(erg): HACK ALERT! This was thrown together for beta and
  // needs to be completely removed after we ship it. Right now, the
  // cut/copy/paste menu items are always enabled and will send a
  // cut/copy/paste command to the currently visible
  // TabContents. Post-beta, this needs to be replaced with a unified
  // interface for supporting cut/copy/paste, and managing who has
  // cut/copy/paste focus. (http://b/1117225)
  virtual void Cut() { }
  virtual void Copy() { }
  virtual void Paste() { }

  // Called on a TabContents when it isn't a popup, but a new window.
  virtual void DisassociateFromPopupCount() { }

  // Window management ---------------------------------------------------------

  // Create a new window constrained to this TabContents' clip and visibility.
  // The window is initialized by using the supplied delegate to obtain basic
  // window characteristics, and the supplied view for the content. The window
  // is sized according to the preferred size of the content_view, and centered
  // within the contents.
  ConstrainedWindow* CreateConstrainedDialog(
      views::WindowDelegate* window_delegate,
      views::View* contents_view);

  // Adds a new tab or window with the given already-created contents
  void AddNewContents(TabContents* new_contents,
                      WindowOpenDisposition disposition,
                      const gfx::Rect& initial_pos,
                      bool user_gesture);

  // Builds a ConstrainedWindow* for the incoming |new_contents| and
  // adds it to child_windows_.
  void AddConstrainedPopup(TabContents* new_contents,
                           const gfx::Rect& initial_pos);

  // When a tab is closed, this method is called for all the remaining tabs. If
  // they all return false or if no tabs are left, the window is closed. The
  // default is to return true
  virtual bool ShouldPreventWindowClose() { return true; }

  // Closes all constrained windows that represent web popups that have not yet
  // been activated by the user and are as such auto-positioned in the bottom
  // right of the screen. This is a quick way for users to "clean up" a flurry
  // of unwanted popups.
  void CloseAllSuppressedPopups();

  // Called when the blocked popup notification is shown or hidden.
  virtual void PopupNotificationVisibilityChanged(bool visible) { }

  // Views and focus -----------------------------------------------------------

  // Returns the actual window that is focused when this TabContents is shown.
  virtual gfx::NativeView GetContentNativeView() {
    return GetNativeView();
  }

  // Tell the subclass to set up the view (e.g. create the container HWND if
  // applicable) and any other create-time setup.
  virtual void CreateView() {}

  // Returns the NativeView associated with this TabContents. Outside of
  // automation in the context of the UI, this is required to be implemented.
  virtual gfx::NativeView GetNativeView() const { return NULL; }

  // Returns the bounds of this TabContents in the screen coordinate system.
  virtual void GetContainerBounds(gfx::Rect *out) const {
    out->SetRect(0, 0, 0, 0);
  }

  // Make the tab the focused window.
  virtual void Focus();

  // Stores the currently focused view.
  virtual void StoreFocus();

  // Restores focus to the last focus view. If StoreFocus has not yet been
  // invoked, SetInitialFocus is invoked.
  virtual void RestoreFocus();

  // Invoked the first time this tab is getting the focus through TAB traversal.
  // By default this does nothing, but is overridden to set the focus for the
  // first element in the page.
  //
  // |reverse| indicates if the user is going forward or backward, so we know
  // whether to set the first or last element focus.
  //
  // See also SetInitialFocus(no arg).
  // FIXME(brettw) having two SetInitialFocus that do different things is silly.
  virtual void SetInitialFocus(bool reverse) { }

  // TabContents that contain View hierarchy (such as NativeUIContents) should
  // return their RootView.  Other TabContents (such as WebContents) should
  // return NULL.
  // This is used by the focus manager to figure out what to focus when the tab
  // is focused (when a tab with no view hierarchy is focused, the
  // TabContentsContainerView is focused) and how to process tab events.  If
  // this returns NULL, the TabContents is supposed to know how to process TAB
  // key events and is just sent the key messages.  If this returns a RootView,
  // the focus is passed to the RootView.
  virtual views::RootView* GetContentsRootView() { return NULL; }

  // Infobars ------------------------------------------------------------------

  // Adds an InfoBar for the specified |delegate|.
  void AddInfoBar(InfoBarDelegate* delegate);

  // Removes the InfoBar for the specified |delegate|.
  void RemoveInfoBar(InfoBarDelegate* delegate);

  // Enumeration and access functions.
  int infobar_delegate_count() const { return infobar_delegates_.size(); }
  InfoBarDelegate* GetInfoBarDelegateAt(int index) {
    return infobar_delegates_.at(index);
  }

  // Toolbars and such ---------------------------------------------------------

  // Returns whether the bookmark bar should be visible.
  virtual bool IsBookmarkBarAlwaysVisible() { return false; }

  // Whether or not the shelf view is visible.
  virtual void SetDownloadShelfVisible(bool visible);
  bool IsDownloadShelfVisible() { return shelf_visible_; }

  // Notify our delegate that some of our content has animated.
  void ToolbarSizeChanged(bool is_animating);

  // Displays the download shelf and animation when a download occurs.
  void OnStartDownload(DownloadItem* download);

  // Returns the DownloadShelf, creating it if necessary.
  DownloadShelf* GetDownloadShelf();

  // Transfer the shelf view from |tab_contents| to the receiving TabContents.
  // |tab_contents| no longer owns the shelf after this call. The shelf is owned
  // by the receiving TabContents.
  void MigrateShelfFrom(TabContents* tab_contents);

  // Migrate the shelf view between 2 TabContents. This helper function is
  // currently called by NavigationController::DiscardPendingEntry. We may
  // want to generalize this if we need to migrate some other state.
  static void MigrateShelf(TabContents* from, TabContents* to);

  // Called when a ConstrainedWindow we own is about to be closed.
  void WillClose(ConstrainedWindow* window);

  // Called when a ConstrainedWindow we own is moved or resized.
  void DidMoveOrResize(ConstrainedWindow* window);

  // Sets focus to the tab contents window, but doesn't actually set focus to
  // a particular element in it (see also SetInitialFocus(bool) which does
  // that in different circumstances).
  // FIXME(brettw) having two SetInitialFocus that do different things is silly.
  virtual void SetInitialFocus();

 protected:
  // NotificationObserver implementation:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  friend class NavigationController;
  // Used to access the child_windows_ (ConstrainedWindowList) for testing
  // automation purposes.
  friend class AutomationProvider;

  explicit TabContents(TabContentsType type);

  // Some tab contents types need to override the type.
  void set_type(TabContentsType type) { type_ = type; }

  // NOTE: the TabContents destructor can run after the NavigationController
  // has gone away, so any complicated unregistering that expects the profile
  // or other shared objects to still be around does not belong in a
  // destructor.
  // For those purposes, instead see Destroy().
  // Protected so that others don't try to delete this directly.
  virtual ~TabContents();

  // Changes the IsLoading state and notifies delegate as needed
  // |details| is used to provide details on the load that just finished
  // (but can be null if not applicable). Can be overridden.
  virtual void SetIsLoading(bool is_loading, LoadNotificationDetails* details);

  // Called by a derived class when the TabContents is resized, causing
  // suppressed constrained web popups to be repositioned to the new bounds
  // if necessary.
  void RepositionSupressedPopupsToFit(const gfx::Size& new_size);

  // Releases the download shelf. This method is used by MigrateShelfFrom.
  // Sub-classes should clear any pointer they might keep to the shelf view and
  // invoke TabContents::ReleaseDownloadShelf().
  virtual void ReleaseDownloadShelf();

  // Called by derived classes to indicate that we're no longer waiting for a
  // response. This won't actually update the throbber, but it will get picked
  // up at the next animation step if the throbber is going.
  void SetNotWaitingForResponse() { waiting_for_response_ = false; }

  typedef std::vector<ConstrainedWindow*> ConstrainedWindowList;
  ConstrainedWindowList child_windows_;

  // Whether we have a notification AND the notification owns popups windows.
  // (We keep the notification object around even when it's not shown since it
  // determines whether to show itself).
  bool ShowingBlockedPopupNotification() const;

 private:
  // Expires InfoBars that need to be expired, according to the state carried
  // in |details|, in response to a new NavigationEntry being committed (the
  // user navigated to another page).
  void ExpireInfoBars(
      const NavigationController::LoadCommittedDetails& details);

  // Data ----------------------------------------------------------------------

  TabContentsType type_;

  TabContentsDelegate* delegate_;
  NavigationController* controller_;

  PropertyBag property_bag_;

  NotificationRegistrar registrar_;

  // Indicates whether we're currently loading a resource.
  bool is_loading_;

  // See is_active() getter above.
  bool is_active_;

  bool is_crashed_;  // true if the tab is considered crashed.

  // See waiting_for_response() above.
  bool waiting_for_response_;

  // The download shelf view (view at the bottom of the page).
  scoped_ptr<DownloadShelf> download_shelf_;

  // Whether the shelf view is visible.
  bool shelf_visible_;

  // Indicates the largest PageID we've seen.  This field is ignored if we are
  // a WebContents, in which case the max page ID is stored separately with
  // each SiteInstance.
  int32 max_page_id_;

  // The id used in the ViewStorage to store the last focused view.
  int last_focused_view_storage_id_;

  // See capturing_contents() above.
  bool capturing_contents_;

  // ConstrainedWindow with additional methods for managing blocked
  // popups. This pointer alsog goes in |child_windows_| for ownership,
  // repositioning, etc.
  BlockedPopupContainer* blocked_popups_;

  // Delegates for InfoBars associated with this TabContents.
  std::vector<InfoBarDelegate*> infobar_delegates_;

  // See getter above.
  bool is_being_destroyed_;

  DISALLOW_COPY_AND_ASSIGN(TabContents);
};

#endif  // CHROME_BROWSER_TAB_CONTENTS_TAB_CONTENTS_H_
