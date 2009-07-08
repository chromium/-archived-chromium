// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_TAB_CONTENTS_H_
#define CHROME_BROWSER_TAB_CONTENTS_TAB_CONTENTS_H_

#include "build/build_config.h"

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/gfx/native_widget_types.h"
#include "base/gfx/rect.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/autocomplete/autocomplete_edit.h"
#include "chrome/browser/cancelable_request.h"
#include "chrome/browser/download/save_package.h"
#include "chrome/browser/fav_icon_helper.h"
#include "chrome/browser/find_notification_details.h"
#include "chrome/browser/shell_dialogs.h"
#include "chrome/browser/renderer_host/render_view_host_delegate.h"
#include "chrome/browser/tab_contents/constrained_window.h"
#include "chrome/browser/tab_contents/infobar_delegate.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/browser/tab_contents/page_navigator.h"
#include "chrome/browser/tab_contents/render_view_host_manager.h"
#include "chrome/common/gears_api.h"
#include "chrome/common/navigation_types.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/page_action.h"
#include "chrome/common/property_bag.h"
#include "net/base/load_states.h"
#include "webkit/glue/password_form.h"
#include "webkit/glue/webpreferences.h"

#if defined(OS_MACOSX) || defined(OS_LINUX)
// Remove when we've finished porting the supporting classes.
#include "chrome/common/temp_scaffolding_stubs.h"
#elif defined(OS_WIN)
#include "chrome/browser/printing/print_view_manager.h"
#endif

namespace gfx {
class Rect;
class Size;
}

namespace views {
class WindowDelegate;
}

namespace base {
class WaitableEvent;
}

namespace webkit_glue {
class AutofillForm;
struct WebApplicationInfo;
}

namespace IPC {
class Message;
}

class AutofillManager;
class BlockedPopupContainer;
class DOMUI;
class DOMUIContents;
class DownloadItem;
class LoadNotificationDetails;
class PageAction;
class PasswordManager;
class PluginInstaller;
class Profile;
struct RendererPreferences;
class RenderViewHost;
class TabContentsDelegate;
class TabContentsFactory;
class SkBitmap;
class SiteInstance;
class TabContentsView;
struct ThumbnailScore;
struct ViewHostMsg_FrameNavigate_Params;
struct ViewHostMsg_DidPrintPage_Params;
class TabContents;

// Describes what goes in the main content area of a tab. TabContents is
// the only type of TabContents, and these should be merged together.
class TabContents : public PageNavigator,
                    public NotificationObserver,
                    public RenderViewHostDelegate,
                    public RenderViewHostDelegate::BrowserIntegration,
                    public RenderViewHostDelegate::Resource,
                    public RenderViewHostManager::Delegate,
                    public SelectFileDialog::Listener {
 public:
  // Flags passed to the TabContentsDelegate.NavigationStateChanged to tell it
  // what has changed. Combine them to update more than one thing.
  enum InvalidateTypes {
    INVALIDATE_URL = 1,           // The URL has changed.
    INVALIDATE_TAB = 2,           // The tab (favicon, title, etc.) has changed
    INVALIDATE_LOAD = 4,          // The loading state has changed.
    INVALIDATE_PAGE_ACTIONS = 8,  // Page action icons have changed.
    // Helper for forcing a refresh.
    INVALIDATE_EVERYTHING = 0xFFFFFFFF
  };

  TabContents(Profile* profile,
              SiteInstance* site_instance,
              int routing_id,
              base::WaitableEvent* modal_dialog_event);
  virtual ~TabContents();

  static void RegisterUserPrefs(PrefService* prefs);

  // Intrinsic tab state -------------------------------------------------------

  // Returns the property bag for this tab contents, where callers can add
  // extra data they may wish to associate with the tab. Returns a pointer
  // rather than a reference since the PropertyAccessors expect this.
  const PropertyBag* property_bag() const { return &property_bag_; }
  PropertyBag* property_bag() { return &property_bag_; }

  // Returns this object as a DOMUIContents if it is one, and NULL otherwise.
  virtual DOMUIContents* AsDOMUIContents() { return NULL; }

  TabContentsDelegate* delegate() const { return delegate_; }
  void set_delegate(TabContentsDelegate* d) { delegate_ = d; }

  // Gets the controller for this tab contents.
  NavigationController& controller() { return controller_; }
  const NavigationController& controller() const { return controller_; }

  // Returns the user profile associated with this TabContents (via the
  // NavigationController).
  Profile* profile() const { return controller_.profile(); }

  // Returns true if contains content rendered by an extension.
  bool HostsExtension() const;

  // Returns the AutofillManager, creating it if necessary.
  AutofillManager* GetAutofillManager();

  // Returns the PasswordManager, creating it if necessary.
  PasswordManager* GetPasswordManager();

  // Returns the PluginInstaller, creating it if necessary.
  PluginInstaller* GetPluginInstaller();

  // Returns the SavePackage which manages the page saving job. May be NULL.
  SavePackage* save_package() const { return save_package_.get(); }

  // Return the currently active RenderProcessHost and RenderViewHost. Each of
  // these may change over time.
  RenderProcessHost* process() const {
    return render_manager_.current_host()->process();
  }
  RenderViewHost* render_view_host() const {
    return render_manager_.current_host();
  }
  // Returns the currently active RenderWidgetHostView. This may change over
  // time and can be NULL (during setup and teardown).
  RenderWidgetHostView* render_widget_host_view() const {
    return render_manager_.current_view();
  }

  // The TabContentsView will never change and is guaranteed non-NULL.
  TabContentsView* view() const {
    return view_.get();
  }

#ifdef UNIT_TEST
  // Expose the render manager for testing.
  RenderViewHostManager* render_manager() { return &render_manager_; }
#endif

  // Tab navigation state ------------------------------------------------------

  // Returns the current navigation properties, which if a navigation is
  // pending may be provisional (e.g., the navigation could result in a
  // download, in which case the URL would revert to what it was previously).
  virtual const GURL& GetURL() const;
  virtual const string16& GetTitle() const;

  // The max PageID of any page that this TabContents has loaded.  PageIDs
  // increase with each new page that is loaded by a tab.  If this is a
  // TabContents, then the max PageID is kept separately on each SiteInstance.
  // Returns -1 if no PageIDs have yet been seen.
  int32 GetMaxPageID();

  // Updates the max PageID to be at least the given PageID.
  void UpdateMaxPageID(int32 page_id);

  // Returns the site instance associated with the current page. By default,
  // there is no site instance. TabContents overrides this to provide proper
  // access to its site instance.
  virtual SiteInstance* GetSiteInstance() const;

  // Initial title assigned to NavigationEntries from Navigate.
  const std::wstring GetDefaultTitle() const;

  // Defines whether this tab's URL should be displayed in the browser's URL
  // bar. Normally this is true so you can see the URL. This is set to false
  // for the new tab page and related pages so that the URL bar is empty and
  // the user is invited to type into it.
  virtual bool ShouldDisplayURL();

  // Returns the favicon for this tab, or an isNull() bitmap if the tab does not
  // have a favicon. The default implementation uses the current navigation
  // entry.
  SkBitmap GetFavIcon() const;

  // Returns whether the favicon should be displayed. If this returns false, no
  // space is provided for the favicon, and the favicon is never displayed.
  virtual bool ShouldDisplayFavIcon();

  // Returns a human-readable description the tab's loading state.
  virtual std::wstring GetStatusText() const;

  // Return whether this tab contents is loading a resource.
  bool is_loading() const { return is_loading_; }

  // Returns whether this tab contents is waiting for a first-response for the
  // main resource of the page. This controls whether the throbber state is
  // "waiting" or "loading."
  bool waiting_for_response() const { return waiting_for_response_; }

  bool is_starred() const { return is_starred_; }

  const std::wstring& encoding() const { return encoding_; }
  void set_encoding(const std::wstring& encoding) {
    encoding_ = encoding;
  }

  // Internal state ------------------------------------------------------------

  // This flag indicates whether the tab contents is currently being
  // screenshotted by the DraggedTabController.
  bool capturing_contents() const { return capturing_contents_; }
  void set_capturing_contents(bool cap) { capturing_contents_ = cap; }

  // Indicates whether this tab should be considered crashed. The setter will
  // also notify the delegate when the flag is changed.
  bool is_crashed() const { return is_crashed_; }
  void SetIsCrashed(bool state);

  // Adds/removes a page action to the list of page actions that are active in
  // this tab. The parameter |title| (if not empty) can be used to override the
  // page action title for this tab and |icon_id| specifies an icon index
  // (defined in the manifest) to use instead of the first icon (for this tab).
  void SetPageActionEnabled(const PageAction* page_action, bool enable,
                            const std::string& title, int icon_id);

  // Returns the page action state for this tab. The pair returns contains
  // the title (string) for the page action and the icon index to use (int).
  // If this function returns NULL it means the page action is not enabled for
  // this tab.
  const PageActionState* GetPageActionState(const PageAction* page_action);

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
  void Activate();

  // TODO(brettw) document these.
  virtual void ShowContents();
  virtual void HideContents();

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
  virtual void Stop();

  // TODO(erg): HACK ALERT! This was thrown together for beta and
  // needs to be completely removed after we ship it. Right now, the
  // cut/copy/paste menu items are always enabled and will send a
  // cut/copy/paste command to the currently visible
  // TabContents. Post-beta, this needs to be replaced with a unified
  // interface for supporting cut/copy/paste, and managing who has
  // cut/copy/paste focus. (http://b/1117225)
  virtual void Cut();
  virtual void Copy();
  virtual void Paste();

  // Called on a TabContents when it isn't a popup, but a new window.
  virtual void DisassociateFromPopupCount();

  // Creates a new TabContents with the same state as this one. The returned
  // heap-allocated pointer is owned by the caller.
  virtual TabContents* Clone();

  // Tell Gears to create a shortcut for the current page.
  void CreateShortcut();

  // Window management ---------------------------------------------------------

#if defined(OS_WIN) || defined(OS_LINUX)
  // Create a new window constrained to this TabContents' clip and visibility.
  // The window is initialized by using the supplied delegate to obtain basic
  // window characteristics, and the supplied view for the content. The window
  // is sized according to the preferred size of the content_view, and centered
  // within the contents.
  ConstrainedWindow* CreateConstrainedDialog(
      ConstrainedWindowDelegate* delegate);
#endif

  // Adds a new tab or window with the given already-created contents
  void AddNewContents(TabContents* new_contents,
                      WindowOpenDisposition disposition,
                      const gfx::Rect& initial_pos,
                      bool user_gesture,
                      const GURL& creator_url);

  // Closes all constrained windows that represent web popups that have not yet
  // been activated by the user and are as such auto-positioned in the bottom
  // right of the screen. This is a quick way for users to "clean up" a flurry
  // of unwanted popups.
  void CloseAllSuppressedPopups();

  // Called when the blocked popup notification is shown or hidden.
  virtual void PopupNotificationVisibilityChanged(bool visible);

  // Returns the number of constrained windows in this tab.  Used by tests.
  size_t constrained_window_count() { return child_windows_.size(); }

  // Views and focus -----------------------------------------------------------
  // TODO(brettw): Most of these should be removed and the caller should call
  // the view directly.

  // Returns the actual window that is focused when this TabContents is shown.
  gfx::NativeView GetContentNativeView();

  // Returns the NativeView associated with this TabContents. Outside of
  // automation in the context of the UI, this is required to be implemented.
  gfx::NativeView GetNativeView() const;

  // Returns the bounds of this TabContents in the screen coordinate system.
  void GetContainerBounds(gfx::Rect *out) const;

  // Makes the tab the focused window.
  void Focus();

  // Focuses the first (last if |reverse| is true) element in the page.
  // Invoked when this tab is getting the focus through tab traversal (|reverse|
  // is true when using Shift-Tab).
  void FocusThroughTabTraversal(bool reverse);

  // Returns true if the location bar should be focused by default rather than
  // the page contents. The view calls this function when the tab is focused
  // to see what it should do.
  bool FocusLocationBarByDefault();

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
  virtual bool IsBookmarkBarAlwaysVisible();

  // Notifies the delegate that a download started.
  void OnStartDownload(DownloadItem* download);

  // Notify our delegate that some of our content has animated.
  void ToolbarSizeChanged(bool is_animating);

  // Called when a ConstrainedWindow we own is about to be closed.
  void WillClose(ConstrainedWindow* window);

  // Called when a BlockedPopupContainer we own is about to be closed.
  void WillCloseBlockedPopupContainer(BlockedPopupContainer* container);

  // Called when a ConstrainedWindow we own is moved or resized.
  void DidMoveOrResize(ConstrainedWindow* window);

  // Interstitials -------------------------------------------------------------

  // Various other systems need to know about our interstitials.
  bool showing_interstitial_page() const {
    return render_manager_.interstitial_page() != NULL;
  }

  // Sets the passed passed interstitial as the currently showing interstitial.
  // |interstitial_page| should be non NULL (use the remove_interstitial_page
  // method to unset the interstitial) and no interstitial page should be set
  // when there is already a non NULL interstitial page set.
  void set_interstitial_page(InterstitialPage* interstitial_page) {
    render_manager_.set_interstitial_page(interstitial_page);
  }

  // Unsets the currently showing interstitial.
  void remove_interstitial_page() {
    render_manager_.remove_interstitial_page();
  }

  // Returns the currently showing interstitial, NULL if no interstitial is
  // showing.
  InterstitialPage* interstitial_page() const {
    return render_manager_.interstitial_page();
  }

  // Find in Page --------------------------------------------------------------

  // Starts the Find operation by calling StartFinding on the Tab. This function
  // can be called from the outside as a result of hot-keys, so it uses the
  // last remembered search string as specified with set_find_string(). This
  // function does not block while a search is in progress. The controller will
  // receive the results through the notification mechanism. See Observe(...)
  // for details.
  void StartFinding(string16 find_text,
                    bool forward_direction,
                    bool case_sensitive);

  // Stops the current Find operation. If |clear_selection| is true, it will
  // also clear the selection on the focused frame.
  void StopFinding(bool clear_selection);

  // Accessors/Setters for find_ui_active_.
  bool find_ui_active() const { return find_ui_active_; }
  void set_find_ui_active(bool find_ui_active) {
      find_ui_active_ = find_ui_active;
  }

  // Setter for find_op_aborted_.
  void set_find_op_aborted(bool find_op_aborted) {
    find_op_aborted_ = find_op_aborted;
  }

  // Used _only_ by testing to get or set the current request ID.
  int current_find_request_id() { return current_find_request_id_; }
  void set_current_find_request_id(int current_find_request_id) {
    current_find_request_id_ = current_find_request_id;
  }

  // Accessor for find_text_. Used to determine if this TabContents has any
  // active searches.
  string16 find_text() const { return find_text_; }

  // Accessor for last_search_prepopulate_text_. Used to access the last search
  // string entered, whatever tab that search was performed in.
  string16 find_prepopulate_text() const {
    return *last_search_prepopulate_text_;
  }

  // Accessor for find_result_.
  const FindNotificationDetails& find_result() const {
    return last_search_result_;
  }

  // Misc state & callbacks ----------------------------------------------------

  // Set whether the contents should block javascript message boxes or not.
  // Default is not to block any message boxes.
  void set_suppress_javascript_messages(bool suppress_javascript_messages) {
    suppress_javascript_messages_ = suppress_javascript_messages;
  }

  // AppModalDialog calls this when the dialog is closed.
  void OnJavaScriptMessageBoxClosed(IPC::Message* reply_msg,
                                    bool success,
                                    const std::wstring& prompt);

  // AppModalDialog calls this when the javascript dialog has been destroyed.
  void OnJavaScriptMessageBoxWindowDestroyed();

  // Prepare for saving the current web page to disk.
  void OnSavePage();

  // Save page with the main HTML file path, the directory for saving resources,
  // and the save type: HTML only or complete web page.
  void SavePage(const std::wstring& main_file,
                const std::wstring& dir_path,
                SavePackage::SavePackageType save_type);

  // Displays asynchronously a print preview (generated by the renderer) if not
  // already displayed and ask the user for its preferred print settings with
  // the "Print..." dialog box. (managed by the print worker thread).
  // TODO(maruel):  Creates a snapshot of the renderer to be used for the new
  // tab for the printing facility.
  void PrintPreview();

  // Prints the current document immediately. Since the rendering is
  // asynchronous, the actual printing will not be completed on the return of
  // this function. Returns false if printing is impossible at the moment.
  bool PrintNow();

  // Notify the completion of a printing job.
  void PrintingDone(int document_cookie, bool success);

  // Returns true if the active NavigationEntry's page_id equals page_id.
  bool IsActiveEntry(int32 page_id);

  const std::string& contents_mime_type() const {
    return contents_mime_type_;
  }

  // Returns true if this TabContents will notify about disconnection.
  bool notify_disconnection() const { return notify_disconnection_; }

  // Override the encoding and reload the page by sending down
  // ViewMsg_SetPageEncoding to the renderer. |UpdateEncoding| is kinda
  // the opposite of this, by which 'browser' is notified of
  // the encoding of the current tab from 'renderer' (determined by
  // auto-detect, http header, meta, bom detection, etc).
  void override_encoding(const std::wstring& encoding) {
    set_encoding(encoding);
    render_view_host()->SetPageEncoding(encoding);
  }

  void WindowMoveOrResizeStarted() {
    render_view_host()->WindowMoveOrResizeStarted();
  }

  BlockedPopupContainer* blocked_popup_container() const {
    return blocked_popups_;
  }

 private:
  friend class NavigationController;
  // Used to access the child_windows_ (ConstrainedWindowList) for testing
  // automation purposes.
  friend class AutomationProvider;
  friend class BlockedPopupContainerTest;
  friend class BlockedPopupContainerControllerTest;

  FRIEND_TEST(BlockedPopupContainerTest, TestReposition);
  FRIEND_TEST(TabContentsTest, NoJSMessageOnInterstitials);
  FRIEND_TEST(TabContentsTest, UpdateTitle);

  // Temporary until the view/contents separation is complete.
  friend class TabContentsView;
#if defined(OS_WIN)
  friend class TabContentsViewWin;
#elif defined(OS_MACOSX)
  friend class TabContentsViewMac;
#elif defined(OS_LINUX)
  friend class TabContentsViewGtk;
#endif

  // So InterstitialPage can access SetIsLoading.
  friend class InterstitialPage;
  // When CreateShortcut is invoked RenderViewHost::GetApplicationInfo is
  // invoked. CreateShortcut caches the state of the page needed to create the
  // shortcut in PendingInstall. When OnDidGetApplicationInfo is invoked, it
  // uses the information from PendingInstall and the WebApplicationInfo
  // to create the shortcut.
  class GearsCreateShortcutCallbackFunctor;
  struct PendingInstall {
    int32 page_id;
    SkBitmap icon;
    std::wstring title;
    GURL url;
    // This object receives the GearsCreateShortcutCallback and routes the
    // message back to the TabContents, if we haven't been deleted.
    GearsCreateShortcutCallbackFunctor* callback_functor;
  };

  // TODO(brettw) TestTabContents shouldn't exist!
  friend class TestTabContents;

  // Changes the IsLoading state and notifies delegate as needed
  // |details| is used to provide details on the load that just finished
  // (but can be null if not applicable). Can be overridden.
  void SetIsLoading(bool is_loading,
                    LoadNotificationDetails* details);

  // Constructs |blocked_popups_| if need be.
  void CreateBlockedPopupContainerIfNecessary();

  // Adds the incoming |new_contents| to the |blocked_popups_| container.
  void AddPopup(TabContents* new_contents,
                const gfx::Rect& initial_pos,
                const std::string& host);

  // Called by a derived class when the TabContents is resized, causing
  // suppressed constrained web popups to be repositioned to the new bounds
  // if necessary.
  void RepositionSupressedPopupsToFit();

  // Whether we have a notification AND the notification owns popups windows.
  // (We keep the notification object around even when it's not shown since it
  // determines whether to show itself).
  bool ShowingBlockedPopupNotification() const;

  // Only used during unit testing; otherwise |blocked_popups_| will be created
  // on demand.
  void set_blocked_popup_container(BlockedPopupContainer* container) {
    DCHECK(blocked_popups_ == NULL);
    blocked_popups_ = container;
  }

  // Called by derived classes to indicate that we're no longer waiting for a
  // response. This won't actually update the throbber, but it will get picked
  // up at the next animation step if the throbber is going.
  void SetNotWaitingForResponse() { waiting_for_response_ = false; }

  typedef std::vector<ConstrainedWindow*> ConstrainedWindowList;
  ConstrainedWindowList child_windows_;

  // Expires InfoBars that need to be expired, according to the state carried
  // in |details|, in response to a new NavigationEntry being committed (the
  // user navigated to another page).
  void ExpireInfoBars(
      const NavigationController::LoadCommittedDetails& details);

  // Called when the user dismisses the shortcut creation dialog.  'success' is
  // true if the shortcut was created.
  void OnGearsCreateShortcutDone(const GearsShortcutData2& shortcut_data,
                                 bool success);


  // Returns the DOMUI for the current state of the tab. This will either be
  // the pending DOMUI, the committed DOMUI, or NULL.
  DOMUI* GetDOMUIForCurrentState();

  // Navigation helpers --------------------------------------------------------
  //
  // These functions are helpers for Navigate() and DidNavigate().

  // Handles post-navigation tasks in DidNavigate AFTER the entry has been
  // committed to the navigation controller. Note that the navigation entry is
  // not provided since it may be invalid/changed after being committed. The
  // current navigation entry is in the NavigationController at this point.
  void DidNavigateMainFramePostCommit(
      const NavigationController::LoadCommittedDetails& details,
      const ViewHostMsg_FrameNavigate_Params& params);
  void DidNavigateAnyFramePostCommit(
      RenderViewHost* render_view_host,
      const NavigationController::LoadCommittedDetails& details,
      const ViewHostMsg_FrameNavigate_Params& params);

  // Closes all child windows (constrained popups) when the domain changes.
  // Supply the new and old URLs, and this function will figure out when the
  // domain changing conditions are met.
  void MaybeCloseChildWindows(const GURL& previous_url,
                              const GURL& current_url);

  // Updates the starred state from the bookmark bar model. If the state has
  // changed, the delegate is notified.
  void UpdateStarredStateForCurrentURL();

  // Send the alternate error page URL to the renderer. This method is virtual
  // so special html pages can override this (e.g., the new tab page).
  virtual void UpdateAlternateErrorPageURL();

  // Send webkit specific settings to the renderer.
  void UpdateWebPreferences();

  // If our controller was restored and the page id is > than the site
  // instance's page id, the site instances page id is updated as well as the
  // renderers max page id.
  void UpdateMaxPageIDIfNecessary(SiteInstance* site_instance,
                                  RenderViewHost* rvh);

  // Called by OnMsgNavigate to update history state. Overridden by subclasses
  // that don't want to be added to history.
  virtual void UpdateHistoryForNavigation(const GURL& display_url,
      const NavigationController::LoadCommittedDetails& details,
      const ViewHostMsg_FrameNavigate_Params& params);

  // Saves the given title to the navigation entry and does associated work. It
  // will update history and the view for the new title, and also synthesize
  // titles for file URLs that have none (so we require that the URL of the
  // entry already be set).
  //
  // This is used as the backend for state updates, which include a new title,
  // or the dedicated set title message. It returns true if the new title is
  // different and was therefore updated.
  bool UpdateTitleForEntry(NavigationEntry* entry, const std::wstring& title);

  // Misc non-view stuff -------------------------------------------------------

  // Helper functions for sending notifications.
  void NotifySwapped();
  void NotifyConnected();
  void NotifyDisconnected();

  // If params has a searchable form, this tries to create a new keyword.
  void GenerateKeywordIfNecessary(
      const ViewHostMsg_FrameNavigate_Params& params);

  // RenderViewHostDelegate ----------------------------------------------------

  // RenderViewHostDelegate::BrowserIntegration implementation.
  virtual void OnUserGesture();
  virtual void OnFindReply(int request_id,
                           int number_of_matches,
                           const gfx::Rect& selection_rect,
                           int active_match_ordinal,
                           bool final_update);
  virtual void GoToEntryAtOffset(int offset);
  virtual void GetHistoryListCount(int* back_list_count,
                                   int* forward_list_count);
  virtual void OnMissingPluginStatus(int status);
  virtual void OnCrashedPlugin(const FilePath& plugin_path);
  virtual void OnCrashedWorker();
  virtual void OnDidGetApplicationInfo(
      int32 page_id,
      const webkit_glue::WebApplicationInfo& info);

  // RenderViewHostDelegate::Resource implementation.
  virtual void DidStartProvisionalLoadForFrame(RenderViewHost* render_view_host,
                                               bool is_main_frame,
                                               const GURL& url);
  virtual void DidStartReceivingResourceResponse(
      ResourceRequestDetails* details);
  virtual void DidRedirectProvisionalLoad(int32 page_id,
                                          const GURL& source_url,
                                          const GURL& target_url);
  virtual void DidRedirectResource(ResourceRequestDetails* details);
  virtual void DidLoadResourceFromMemoryCache(
      const GURL& url,
      const std::string& frame_origin,
      const std::string& main_frame_origin,
      const std::string& security_info);
  virtual void DidFailProvisionalLoadWithError(
      RenderViewHost* render_view_host,
      bool is_main_frame,
      int error_code,
      const GURL& url,
      bool showing_repost_interstitial);
  virtual void DocumentLoadedInFrame();

  // RenderViewHostDelegate implementation.
  virtual RenderViewHostDelegate::View* GetViewDelegate() const;
  virtual RenderViewHostDelegate::BrowserIntegration*
      GetBrowserIntegrationDelegate() const;
  virtual RenderViewHostDelegate::Resource* GetResourceDelegate() const;
  virtual RenderViewHostDelegate::Save* GetSaveDelegate() const;
  virtual RenderViewHostDelegate::FavIcon* GetFavIconDelegate() const;
  virtual TabContents* GetAsTabContents();
  virtual void RenderViewCreated(RenderViewHost* render_view_host);
  virtual void RenderViewReady(RenderViewHost* render_view_host);
  virtual void RenderViewGone(RenderViewHost* render_view_host);
  virtual void DidNavigate(RenderViewHost* render_view_host,
                           const ViewHostMsg_FrameNavigate_Params& params);
  virtual void UpdateState(RenderViewHost* render_view_host,
                           int32 page_id,
                           const std::string& state);
  virtual void UpdateTitle(RenderViewHost* render_view_host,
                           int32 page_id,
                           const std::wstring& title);
  virtual void UpdateEncoding(RenderViewHost* render_view_host,
                              const std::wstring& encoding);
  virtual void UpdateTargetURL(int32 page_id, const GURL& url);
  virtual void UpdateThumbnail(const GURL& url,
                               const SkBitmap& bitmap,
                               const ThumbnailScore& score);
  virtual void UpdateInspectorSettings(const std::wstring& raw_settings);
  virtual void Close(RenderViewHost* render_view_host);
  virtual void RequestMove(const gfx::Rect& new_bounds);
  virtual void DidStartLoading(RenderViewHost* render_view_host);
  virtual void DidStopLoading(RenderViewHost* render_view_host);
  virtual void RequestOpenURL(const GURL& url, const GURL& referrer,
                              WindowOpenDisposition disposition);
  virtual void DomOperationResponse(const std::string& json_string,
                                    int automation_id);
  virtual void ProcessDOMUIMessage(const std::string& message,
                                   const std::string& content,
                                   int request_id,
                                   bool has_callback);
  virtual void ProcessExternalHostMessage(const std::string& message,
                                          const std::string& origin,
                                          const std::string& target);
  virtual void RunFileChooser(bool multiple_files,
                              const string16& title,
                              const FilePath& default_file);
  virtual void RunJavaScriptMessage(const std::wstring& message,
                                    const std::wstring& default_prompt,
                                    const GURL& frame_url,
                                    const int flags,
                                    IPC::Message* reply_msg,
                                    bool* did_suppress_message);
  virtual void RunBeforeUnloadConfirm(const std::wstring& message,
                                      IPC::Message* reply_msg);
  virtual void ShowModalHTMLDialog(const GURL& url, int width, int height,
                                   const std::string& json_arguments,
                                   IPC::Message* reply_msg);
  virtual void PasswordFormsSeen(
      const std::vector<webkit_glue::PasswordForm>& forms);
  virtual void AutofillFormSubmitted(const webkit_glue::AutofillForm& form);
  virtual void GetAutofillSuggestions(const std::wstring& field_name,
      const std::wstring& user_text, int64 node_id, int request_id);
  virtual void RemoveAutofillEntry(const std::wstring& field_name,
                                   const std::wstring& value);
  virtual void PageHasOSDD(RenderViewHost* render_view_host,
                           int32 page_id, const GURL& url, bool autodetected);
  virtual void DidGetPrintedPagesCount(int cookie, int number_pages);
  virtual void DidPrintPage(const ViewHostMsg_DidPrintPage_Params& params);
  virtual GURL GetAlternateErrorPageURL() const;
  virtual RendererPreferences GetRendererPrefs() const;
  virtual WebPreferences GetWebkitPrefs();
  virtual void OnJSOutOfMemory();
  virtual void ShouldClosePage(bool proceed);
  virtual void OnCrossSiteResponse(int new_render_process_host_id,
                                   int new_request_id);
  virtual void OnCrossSiteNavigationCanceled();
  virtual bool CanBlur() const;
  virtual gfx::Rect GetRootWindowResizerRect() const;
  virtual void RendererUnresponsive(RenderViewHost* render_view_host,
                                    bool is_during_unload);
  virtual void RendererResponsive(RenderViewHost* render_view_host);
  virtual void LoadStateChanged(const GURL& url, net::LoadState load_state);
  virtual bool IsExternalTabContainer() const;
  virtual void DidInsertCSS();

  // SelectFileDialog::Listener ------------------------------------------------

  virtual void FileSelected(const FilePath& path, int index, void* params);
  virtual void MultiFilesSelected(const std::vector<FilePath>& files,
                                  void* params);
  virtual void FileSelectionCanceled(void* params);

  // RenderViewHostManager::Delegate -------------------------------------------

  virtual void BeforeUnloadFiredFromRenderManager(
      bool proceed,
      bool* proceed_to_fire_unload);
  virtual void DidStartLoadingFromRenderManager(
      RenderViewHost* render_view_host) {
    DidStartLoading(render_view_host);
  }
  virtual void RenderViewGoneFromRenderManager(
      RenderViewHost* render_view_host) {
    RenderViewGone(render_view_host);
  }
  virtual void UpdateRenderViewSizeForRenderManager();
  virtual void NotifySwappedFromRenderManager() {
    NotifySwapped();
  }
  virtual NavigationController& GetControllerForRenderManager() {
    return controller();
  }
  virtual DOMUI* CreateDOMUIForRenderManager(const GURL& url);
  virtual NavigationEntry* GetLastCommittedNavigationEntryForRenderManager();

  // Initializes the given renderer if necessary and creates the view ID
  // corresponding to this view host. If this method is not called and the
  // process is not shared, then the TabContents will act as though the renderer
  // is not running (i.e., it will render "sad tab"). This method is
  // automatically called from LoadURL.
  //
  // If you are attaching to an already-existing RenderView, you should call
  // InitWithExistingID.
  virtual bool CreateRenderViewForRenderManager(
      RenderViewHost* render_view_host);

  // NotificationObserver ------------------------------------------------------

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Data for core operation ---------------------------------------------------

  // Delegate for notifying our owner about stuff. Not owned by us.
  TabContentsDelegate* delegate_;

  // Handles the back/forward list and loading.
  NavigationController controller_;

  // The corresponding view.
  scoped_ptr<TabContentsView> view_;

  // Helper classes ------------------------------------------------------------

  // Manages creation and swapping of render views.
  RenderViewHostManager render_manager_;

  // Stores random bits of data for others to associate with this object.
  PropertyBag property_bag_;

  // Registers and unregisters us for notifications.
  NotificationRegistrar registrar_;

  // Handles print preview and print job for this contents.
  printing::PrintViewManager printing_;

  // SavePackage, lazily created.
  scoped_refptr<SavePackage> save_package_;

  // Tracks our pending CancelableRequests. This maps pending requests to
  // page IDs so that we know whether a given callback still applies. The
  // page ID -1 means no page ID was set.
  CancelableRequestConsumerT<int32, -1> cancelable_consumer_;

  // AutofillManager, lazily created.
  scoped_ptr<AutofillManager> autofill_manager_;

  // PasswordManager, lazily created.
  scoped_ptr<PasswordManager> password_manager_;

  // PluginInstaller, lazily created.
  scoped_ptr<PluginInstaller> plugin_installer_;

  // Handles downloading favicons.
  FavIconHelper fav_icon_helper_;

  // Dialog box used for choosing files to upload from file form fields.
  scoped_refptr<SelectFileDialog> select_file_dialog_;

  // Web app installation.
  PendingInstall pending_install_;

  // Data for loading state ----------------------------------------------------

  // Indicates whether we're currently loading a resource.
  bool is_loading_;

  // Indicates if the tab is considered crashed.
  bool is_crashed_;

  // See waiting_for_response() above.
  bool waiting_for_response_;

  // Indicates the largest PageID we've seen.  This field is ignored if we are
  // a TabContents, in which case the max page ID is stored separately with
  // each SiteInstance.
  // TODO(brettw) this seems like it can be removed according to the comment.
  int32 max_page_id_;

  // System time at which the current load was started.
  base::TimeTicks current_load_start_;

  // The current load state and the URL associated with it.
  net::LoadState load_state_;
  std::wstring load_state_host_;

  // Data for current page -----------------------------------------------------

  // Whether we have a (non-empty) title for the current page.
  // Used to prevent subsequent title updates from affecting history. This
  // prevents some weirdness because some AJAXy apps use titles for status
  // messages.
  bool received_page_title_;

  // Whether the current URL is starred
  bool is_starred_;

  // When a navigation occurs, we record its contents MIME type. It can be
  // used to check whether we can do something for some special contents.
  std::string contents_mime_type_;

  // Character encoding. TODO(jungshik) : convert to std::string
  std::wstring encoding_;

  // Data for shelves and stuff ------------------------------------------------

  // ConstrainedWindow with additional methods for managing blocked
  // popups.
  BlockedPopupContainer* blocked_popups_;

  // Delegates for InfoBars associated with this TabContents.
  std::vector<InfoBarDelegate*> infobar_delegates_;

  // Data for find in page -----------------------------------------------------

  // TODO(brettw) this should be separated into a helper class.

  // Each time a search request comes in we assign it an id before passing it
  // over the IPC so that when the results come in we can evaluate whether we
  // still care about the results of the search (in some cases we don't because
  // the user has issued a new search).
  static int find_request_id_counter_;

  // True if the Find UI is active for this Tab.
  bool find_ui_active_;

  // True if a Find operation was aborted. This can happen if the Find box is
  // closed or if the search term inside the Find box is erased while a search
  // is in progress. This can also be set if a page has been reloaded, and will
  // on FindNext result in a full Find operation so that the highlighting for
  // inactive matches can be repainted.
  bool find_op_aborted_;

  // This variable keeps track of what the most recent request id is.
  int current_find_request_id_;

  // The last string we searched for. This is used to figure out if this is a
  // Find or a FindNext operation (FindNext should not increase the request id).
  string16 find_text_;

  // Whether the last search was case sensitive or not.
  bool last_search_case_sensitive_;

  // Keeps track of the last search string that was used to search in any tab.
  string16* last_search_prepopulate_text_;

  // The last find result. This object contains details about the number of
  // matches, the find selection rectangle, etc. The UI can access this
  // information to build its presentation.
  FindNotificationDetails last_search_result_;

  // Data for Page Actions -----------------------------------------------------

  // A map of page actions that are enabled in this tab (and a state object
  // that can be used to override the title and icon used for the page action).
  // This map is cleared every time the mainframe navigates and populated by the
  // PageAction extension API.
  std::map< const PageAction*, linked_ptr<PageActionState> >
      enabled_page_actions_;

  // Data for misc internal state ----------------------------------------------

  // See capturing_contents() above.
  bool capturing_contents_;

  // See getter above.
  bool is_being_destroyed_;

  // Indicates whether we should notify about disconnection of this
  // TabContents. This is used to ensure disconnection notifications only
  // happen if a connection notification has happened and that they happen only
  // once.
  bool notify_disconnection_;

  // Maps from handle to page_id.
  typedef std::map<HistoryService::Handle, int32> HistoryRequestMap;
  HistoryRequestMap history_requests_;

#if defined(OS_WIN)
  // Handle to an event that's set when the page is showing a message box (or
  // equivalent constrained window).  Plugin processes check this to know if
  // they should pump messages then.
  ScopedHandle message_box_active_;
#endif

  // The time that the last javascript message was dismissed.
  base::TimeTicks last_javascript_message_dismissal_;

  // True if the user has decided to block future javascript messages. This is
  // reset on navigations to false on navigations.
  bool suppress_javascript_messages_;

  // ---------------------------------------------------------------------------

  DISALLOW_COPY_AND_ASSIGN(TabContents);
};

#endif  // CHROME_BROWSER_TAB_CONTENTS_TAB_CONTENTS_H_
