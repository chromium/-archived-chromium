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

#ifndef CHROME_BROWSER_TAB_CONTENTS_H_
#define CHROME_BROWSER_TAB_CONTENTS_H_

#include <string>
#include <vector>

#include "chrome/browser/autocomplete/autocomplete_edit.h"
#include "chrome/browser/constrained_window.h"
#include "chrome/browser/navigation_controller.h"
#include "chrome/browser/page_navigator.h"
#include "chrome/browser/tab_contents_type.h"
#include "chrome/common/navigation_types.h"
#include "chrome/common/text_zoom.h"

namespace gfx {
  class Rect;
  class Size;
}

class DOMUIHost;
class DownloadItem;
class DownloadShelfView;
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
//
class TabContents : public PageNavigator,
                    public ConstrainedTabContentsDelegate,
                    public NotificationObserver {
  // Used to access the child_windows_ (ConstrainedWindowList) for testing
  // automation purposes.
  friend class AutomationProvider;

 public:
  // Flags passed to the TabContentsDelegate.NavigationStateChanged to tell it
  // what has changed. Combine them to update more than one thing.
  enum InvalidateTypes {
    INVALIDATE_URL = 1,      // The URL has changed.
    INVALIDATE_TITLE = 2,    // The title has changed.
    INVALIDATE_FAVICON = 4,  // The favicon has changed.
    INVALIDATE_STATE = 8,    // Forms, scroll position, etc.) have changed.
    INVALIDATE_LOAD = 16,    // The loading state has changed

    // Helper for forcing a refresh.
    INVALIDATE_EVERYTHING = 0xFFFFFFFF
  };

  // Creates a new TabContents of the given type.  Will reuse the given
  // instance's renderer, if it is not null.
  static TabContents* CreateWithType(TabContentsType type,
                                     HWND parent,
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

  // Tell the subclass to set up the view (e.g. create the container HWND if
  // applicable) and any other create-time setup.
  virtual void CreateView(HWND parent_hwnd, const gfx::Rect& initial_bounds) {}

  // Returns the HWND associated with this TabContents. Outside of automation
  // in the context of the UI, this is required to be implemented.
  virtual HWND GetContainerHWND() const { return NULL; }

  // Returns the bounds of this TabContents in the screen coordinate system.
  virtual void GetContainerBounds(gfx::Rect *out) const {
    out->SetRect(0, 0, 0, 0);
  }

  // Show, Hide and Size the TabContents.
  // TODO(beng): (Cleanup) Show/Size TabContents should be made to actually
  //             show and size the View. For simplicity sake, for now they're
  //             just empty. This is currently a bit of a mess and is just a
  //             band-aid.
  virtual void ShowContents() {}
  virtual void HideContents();
  virtual void SizeContents(const gfx::Size& size) {}

  TabContentsType type() const { return type_; }

  // Returns this object as a WebContents if it is one, and NULL otherwise.
  virtual WebContents* AsWebContents() { return NULL; }

  // Const version of above for situations where const TabContents*'s are used.
  WebContents* AsWebContents() const {
    return const_cast<TabContents*>(this)->AsWebContents();
  }

  // Returns this object as a DOMUIHost if it is one, and NULL otherwise.
  virtual DOMUIHost* AsDOMUIHost() { return NULL ; }

  // The max PageID of any page that this TabContents has loaded.  PageIDs
  // increase with each new page that is loaded by a tab.  If this is a
  // WebContents, then the max PageID is kept separately on each SiteInstance.
  // Returns -1 if no PageIDs have yet been seen.
  int32 GetMaxPageID();

  // Updates the max PageID to be at least the given PageID.
  void UpdateMaxPageID(int32 page_id);

  // Initial title assigned to NavigationEntries from Navigate.
  virtual const std::wstring GetDefaultTitle() const;

  // Defines whether the url should be displayed within the browser. If false
  // is returned, the URL field is blank and grabs focus to invite the user
  // to type a new url
  virtual bool ShouldDisplayURL() { return true; }

  // Returns the favicon for this tab, or an isNull() bitmap if the tab does not
  // have a favicon. The default implementation uses the current navigation
  // entry.
  virtual SkBitmap GetFavIcon() const;

  // Returns whether the favicon should be displayed. If this returns false, no
  // space is provided for the favicon, and the favicon is never displayed.
  virtual bool ShouldDisplayFavIcon() {
    return true;
  }

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

  // For use when switching tabs, these functions allow the tab contents to
  // hold the per-tab state of the location bar.  The tab contents takes
  // ownership of the pointer.
  void set_saved_location_bar_state(const AutocompleteEdit::State* state) {
    saved_location_bar_state_.reset(state);
  }
  const AutocompleteEdit::State* saved_location_bar_state() const {
    return saved_location_bar_state_.get();
  }

  // Returns the current navigation properties, which if a navigation is
  // pending may be provisional (e.g., the navigation could result in a
  // download, in which case the URL would revert to what it was previously).
  const GURL& GetURL() const;
  virtual const std::wstring& GetTitle() const;

  // SSL related states.
  SecurityStyle GetSecurityStyle() const;

  // Sets |ev_text| to the text that should be displayed in the EV label of
  // the location bar and |ev_tooltip_text| to the tooltip for that label.
  // Returns false and sets these strings to empty if the current page is either
  // not served over HTTPS or if HTTPS does not use an EV cert.
  bool GetSSLEVText(std::wstring* ev_text, std::wstring* ev_tooltip_text) const;

  // Request this tab to shut down.
  // This kills the tab's NavigationController, which then Destroy()s all
  // tabs it controls.
  void CloseContents();

  // Unregister/shut down any pending tasks involving this tab.
  // This is called as the tab is shutting down, before the
  // NavigationController (and consequently profile) are gone.
  //
  // If you override this, be sure to call this implementation at the end
  // of yours.
  // See also Close().
  virtual void Destroy();

  // Create a new window constrained to this TabContents' clip and visibility.
  // The window is initialized by using the supplied delegate to obtain basic
  // window characteristics, and the supplied view for the content. The window
  // is sized according to the preferred size of the content_view, and centered
  // within the contents.
  ConstrainedWindow* CreateConstrainedDialog(
      ChromeViews::WindowDelegate* window_delegate,
      ChromeViews::View* contents_view);

  // Adds a new tab or window with the given already-created contents
  void AddNewContents(TabContents* new_contents,
                      WindowOpenDisposition disposition,
                      const gfx::Rect& initial_pos,
                      bool user_gesture);

  // Builds a ConstrainedWindow* for the incoming |new_contents| and
  // adds it to child_windows_.
  void AddConstrainedPopup(TabContents* new_contents,
                           const gfx::Rect& initial_pos);

  // An asynchronous call to trigger the string search in the page.
  // It sends an IPC message to the Renderer that handles the string
  // search, selecting the matches and setting the caret positions.
  // This function also starts the asynchronous scoping effort.
  virtual void StartFinding(int request_id,
                            const std::wstring& string,
                            bool forward, bool match_case,
                            bool find_next) { }

  // An asynchronous call to stop the string search in the page. If
  // |clear_selection| is true, it will also clear the selection on the
  // focused frame.
  virtual void StopFinding(bool clear_selection) { }

  // Asynchronous calls to change the text zoom level.
  virtual void AlterTextSize(text_zoom::TextSize size) { }

  // Asynchronous call to turn on/off encoding auto detector.
  virtual void SetEncodingAutoDetector(bool encoding_auto_detector) { }

  // Asynchronous call to change page encoding.
  virtual void SetPageEncoding(const std::string& encoding_name) { }

  // Return whether this tab contents is loading a resource.
  bool is_loading() const { return is_loading_; }

  // Returns whether this tab contents is waiting for an first-response from
  // and external resource.
  bool response_started() const { return response_started_; }

  // Set whether this tab contents is active. A tab content is active for a
  // given tab if it is currently being used to display some contents. Note that
  // this is different from whether a tab is selected.
  virtual void SetActive(bool active) { is_active_ = active; }
  bool is_active() const { return is_active_; }

  // Called by the NavigationController to cause the TabContents to navigate to
  // the specified entry.  Either TabContents::DidNavigateToEntry or the
  // navigation controller's DiscardPendingEntry method should be called in
  // response (possibly sometime later).
  //
  // The entry has a PageID of -1 if newly created (corresponding to navigation
  // to a new URL).
  //
  // If this method returns false, then the navigation is discarded (equivalent
  // to calling DiscardPendingEntry on the NavigationController).
  //
  virtual bool Navigate(const NavigationEntry& entry, bool reload);

  // Stop any pending navigation.
  virtual void Stop() {}

  // Convenience method for notifying the delegate of a navigation state
  // change. See TabContentsDelegate.
  void NotifyNavigationStateChanged(unsigned changed_flags);

  // Convenience method for notifying the delegate of a navigation.
  // See TabContentsDelegate.
  void NotifyDidNavigate(NavigationType nav_type,
                         int relative_navigation_offset);

  // Invoked when the tab contents becomes selected. If you override, be sure
  // and invoke super's implementation.
  virtual void DidBecomeSelected();

  // Invoked when the tab contents becomes hidden.
  // NOTE: If you override this, call the superclass version too!
  virtual void WasHidden();

  // Activates this contents within its containing window, bringing that window
  // to the foreground if necessary.
  virtual void Activate();

  // Closes all constrained windows that represent web popups that have not yet
  // been activated by the user and are as such auto-positioned in the bottom
  // right of the screen. This is a quick way for users to "clean up" a flurry
  // of unwanted popups.
  void CloseAllSuppressedPopups();

  // Displays the download shelf and animation when a download occurs.
  void OnStartDownload(DownloadItem* download);

  // ConstrainedTabContentsDelegate methods:
  virtual void AddNewContents(ConstrainedWindow* window,
                              TabContents* contents,
                              WindowOpenDisposition disposition,
                              const gfx::Rect& initial_pos,
                              bool user_gesture);
  virtual void OpenURL(ConstrainedWindow* window,
                       const GURL& url,
                       WindowOpenDisposition disposition,
                       PageTransition::Type transition);
  virtual void WillClose(ConstrainedWindow* window);
  virtual void DetachContents(ConstrainedWindow* window,
                              TabContents* contents,
                              const gfx::Rect& contents_bounds,
                              const gfx::Point& mouse_pt,
                              int frame_component);
  virtual void DidMoveOrResize(ConstrainedWindow* window);

  // Returns the actual window that is focused when this TabContents is shown.
  virtual HWND GetContentHWND() {
    return GetContainerHWND();
  }

  // PageNavigator methods:
  virtual void OpenURL(const GURL& url,
                       WindowOpenDisposition disposition,
                       PageTransition::Type transition);

  virtual void OpenURLWithOverrideEncoding(
      const GURL& url,
      WindowOpenDisposition disposition,
      PageTransition::Type transition,
      const std::string& override_encoding);

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) { }

  // Make the tab the focused window.
  virtual void Focus();

  // Stores the currently focused view.
  virtual void StoreFocus();

  // Restores focus to the last focus view. If StoreFocus has not yet been
  // invoked, SetInitialFocus is invoked.
  virtual void RestoreFocus();

  // When a tab is closed, this method is called for all the remaining tabs. If
  // they all return false or if no tabs are left, the window is closed. The
  // default is to return true
  virtual bool ShouldPreventWindowClose() {
    return true;
  }

  // Returns the View to display at the top of the tab.
  virtual InfoBarView* GetInfoBarView() { return NULL; }

  // Returns whether the info bar is visible.
  // If the visibility dynamically changes, invoke ToolbarSizeChanged
  // on the delegate. Which forces the frame to layout if size has changed.
  virtual bool IsInfoBarVisible() { return false; }

  // TabContents that contain View hierarchy (such as NativeUIContents) should
  // return their RootView.  Other TabContents (such as WebContents) should
  // return NULL.
  // This is used by the focus manager to figure out what to focus when the tab
  // is focused (when a tab with no view hierarchy is focused, the
  // TabContentsContainerView is focused) and how to process tab events.  If
  // this returns NULL, the TabContents is supposed to know how to process TAB
  // key events and is just sent the key messages.  If this returns a RootView,
  // the focus is passed to the RootView.
  virtual ChromeViews::RootView* GetContentsRootView() { return NULL; }

  // Invoked the first time this tab is getting the focus through TAB traversal.
  virtual void SetInitialFocus(bool reverse) { }

  // Returns whether the bookmark bar should be visible.
  virtual bool IsBookmarkBarAlwaysVisible() { return false; }

  // Called before and after capturing an image of this tab contents.  The tab
  // contents may be temporarily re-parented after WillCaptureContents.
  virtual void WillCaptureContents() {}
  virtual void DidCaptureContents() {}

  // Returns true if the tab is currently loading a resource.
  virtual bool IsLoading() const { return is_loading_; }

  // Returns a human-readable description the tab's loading state.
  virtual std::wstring GetStatusText() const { return std::wstring(); }

  const std::string& GetEncoding() { return encoding_name_; }
  void SetEncoding(const std::string& encoding_name) {
    encoding_name_ = encoding_name;
  }

  // Changes the IsCrashed state and notifies the delegate as needed.
  void SetIsCrashed(bool state);

  // Return whether this tab should be considered crashed.
  bool IsCrashed() const;

  // Returns whether this tab contents supports the provided URL. By default,
  // this method matches the tab contents type with the result of TypeForURL().
  // |url| points to the actual URL that will be used. It can be modified as
  // needed.
  // Override this method if your TabContents subclass supports various URL
  // schemes but doesn't want to be the default handler for these schemes.
  // For example, the NewTabUIContents overrides this method to support
  // javascript: URLs.
  virtual bool SupportsURL(GURL* url);

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

  // Whether or not the shelf view is visible.
  virtual void SetDownloadShelfVisible(bool visible);
  bool IsDownloadShelfVisible() { return shelf_visible_; }

  // Notify our delegate that some of our content has animated.
  void ToolbarSizeChanged(bool is_animating);

  // Returns the DownloadShelfView, creating it if necessary.
  DownloadShelfView* GetDownloadShelfView();

  // Transfer the shelf view from |tab_contents| to the receiving TabContents.
  // |tab_contents| no longer owns the shelf after this call. The shelf is owned
  // by the receiving TabContents.
  void MigrateShelfViewFrom(TabContents* tab_contents);

  // Migrate the shelf view between 2 TabContents. This helper function is
  // currently called by NavigationController::DiscardPendingEntry. We may
  // want to generalize this if we need to migrate some other state.
  static void MigrateShelfView(TabContents* from, TabContents* to);

  static void RegisterUserPrefs(PrefService* prefs);

 protected:
  friend class NavigationController;

  explicit TabContents(TabContentsType type);

  // NOTE: the TabContents destructor can run after the NavigationController
  // has gone away, so any complicated unregistering that expects the profile
  // or other shared objects to still be around does not belong in a
  // destructor.
  // For those purposes, instead see Destroy().
  // Protected so that others don't try to delete this directly.
  virtual ~TabContents();

  // Set focus on the initial component. This is invoked from
  // RestoreFocus if SetLastFocusOwner has not yet been invoked.
  virtual void SetInitialFocus();

  // Changes the IsLoading state and notifies delegate as needed
  // |details| is used to provide details on the load that just finished
  // (but can be null if not applicable)
  void SetIsLoading(bool is_loading, LoadNotificationDetails* details);

  // Called by subclasses when a navigation occurs.  Ownership of the entry
  // object is passed to this method.
  void DidNavigateToEntry(NavigationEntry* entry);

  // Called by a derived class when the TabContents is resized, causing
  // suppressed constrained web popups to be repositioned to the new bounds
  // if necessary.
  void RepositionSupressedPopupsToFit(const gfx::Size& new_size);

  // Releases the download shelf. This method is used by MigrateShelfViewFrom.
  // Sub-classes should clear any pointer they might keep to the shelf view and
  // invoke TabContents::ReleaseDownloadShelfView().
  virtual void ReleaseDownloadShelfView();

  bool is_loading_;  // true if currently loading a resource.
  bool response_started_;  // true if waiting for a response.
  bool is_active_;
  bool is_crashed_;  // true if the tab is considered crashed.

  typedef std::vector<ConstrainedWindow*> ConstrainedWindowList;
  ConstrainedWindowList child_windows_;

  TabContentsType type_;

 private:
  ConstrainedWindowList child_windows() const { return child_windows_; }

  // Clear the last focus view and unregister the notification associated with
  // it.
  void ClearLastFocusedView();

  TabContentsDelegate* delegate_;
  NavigationController* controller_;

  scoped_ptr<const AutocompleteEdit::State> saved_location_bar_state_;

  // The download shelf view (view at the bottom of the page).
  scoped_ptr<DownloadShelfView> download_shelf_view_;

  // Whether the shelf view is visible.
  bool shelf_visible_;

  // Indicates the largest PageID we've seen.  This field is ignored if we are
  // a WebContents, in which case the max page ID is stored separately with
  // each SiteInstance.
  int32 max_page_id_;

  // The id used in the ViewStorage to store the last focused view.
  int last_focused_view_storage_id_;

  std::string encoding_name_;
};

#endif  // CHROME_BROWSER_TAB_CONTENTS_H_
