// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_H_
#define CHROME_BROWSER_BROWSER_H_

#include <set>
#include <vector>

#include "base/basictypes.h"
#include "base/gfx/rect.h"
#include "base/scoped_ptr.h"
#include "base/task.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/sessions/session_id.h"
#include "chrome/browser/shell_dialogs.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/tab_contents/page_navigator.h"
#include "chrome/browser/tab_contents/tab_contents_delegate.h"
#include "chrome/browser/toolbar_model.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/pref_member.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

class BrowserIdleTimer;
class BrowserWindow;
class DebuggerWindow;
class FindBarController;
class GoButton;
class LocationBar;
class PrefService;
class Profile;
class SkBitmap;
class StatusBubble;
class TabNavigation;

class Browser : public TabStripModelDelegate,
                public TabStripModelObserver,
                public TabContentsDelegate,
                public PageNavigator,
                public CommandUpdater::CommandUpdaterDelegate,
                public NotificationObserver,
                public SelectFileDialog::Listener {
 public:
  enum Type {
    TYPE_NORMAL = 0,
    TYPE_POPUP = 1,
    TYPE_APP = 2,
    TYPE_APP_POPUP = TYPE_APP | TYPE_POPUP,
  };

  // Possible elements of the Browser window.
  enum WindowFeature {
    FEATURE_TITLEBAR = 1,
    FEATURE_TABSTRIP = 2,
    FEATURE_TOOLBAR = 4,
    FEATURE_LOCATIONBAR = 8,
    FEATURE_BOOKMARKBAR = 16,
    FEATURE_INFOBAR = 32,
    FEATURE_DOWNLOADSHELF = 64,
    FEATURE_EXTENSIONSHELF = 128
  };

  // Maximized state on creation.
  enum MaximizedState {
    // The maximized state is set to the default, which varies depending upon
    // what the user has done.
    MAXIMIZED_STATE_DEFAULT,

    // Maximized state is explicitly maximized.
    MAXIMIZED_STATE_MAXIMIZED,

    // Maximized state is explicitly not maximized (normal).
    MAXIMIZED_STATE_UNMAXIMIZED
  };

  // Constructors, Creation, Showing //////////////////////////////////////////

  // Creates a new browser of the given |type| and for the given |profile|. The
  // Browser has a NULL window after its construction, CreateBrowserWindow must
  // be called after configuration for window() to be valid.
  Browser(Type type, Profile* profile);
  virtual ~Browser();

  // Creates a normal tabbed browser with the specified profile. The Browser's
  // window is created by this function call.
  static Browser* Create(Profile* profile);

  // Like Create, but creates a tabstrip-less popup window.
  static Browser* CreateForPopup(Profile* profile);

  // Like Create, but creates a tabstrip-less and toolbar-less "app" window for
  // the specified app. Passing popup=true will create a TYPE_APP_POPUP browser
  static Browser* CreateForApp(const std::wstring& app_name, Profile* profile,
                               bool is_popup);

  // Set overrides for the initial window bounds and maximized state.
  void set_override_bounds(const gfx::Rect& bounds) {
    override_bounds_ = bounds;
  }
  void set_maximized_state(MaximizedState state) {
    maximized_state_ = state;
  }

  // Creates the Browser Window. Prefer to use the static helpers above where
  // possible. This does not show the window. You need to call window()->Show()
  // to show it.
  void CreateBrowserWindow();

  // Accessors ////////////////////////////////////////////////////////////////

  Type type() const { return type_; }
  Profile* profile() const { return profile_; }
  const std::vector<std::wstring>& user_data_dir_profiles() const;

#if defined(UNIT_TEST)
  // Sets the BrowserWindow. This is intended for testing and generally not
  // useful outside of testing. Use CreateBrowserWindow outside of testing, or
  // the static convenience methods that create a BrowserWindow for you.
  void set_window(BrowserWindow* window) {
    DCHECK(!window_);
    window_ = window;
  }
#endif

  BrowserWindow* window() const { return window_; }
  ToolbarModel* toolbar_model() { return &toolbar_model_; }
  const SessionID& session_id() const { return session_id_; }
  CommandUpdater* command_updater() { return &command_updater_; }
  FindBarController* find_bar() { return find_bar_controller_.get(); }

  // Setters /////////////////////////////////////////////////////////////////

  void set_user_data_dir_profiles(const std::vector<std::wstring>& profiles);

  // Browser Creation Helpers /////////////////////////////////////////////////

  // Opens a new window with the default blank tab.
  static void OpenEmptyWindow(Profile* profile);

  // Opens a new window with the tabs from |profile|'s TabRestoreService.
  static void OpenWindowWithRestoredTabs(Profile* profile);

  // Opens the specified URL in a new browser window in an incognito session.
  // If there is already an existing active incognito session for the specified
  // |profile|, that session is re-used.
  static void OpenURLOffTheRecord(Profile* profile, const GURL& url);

  // Opens the a new application ("thin frame") window for the specified url.
  static void OpenApplicationWindow(Profile* profile, const GURL& url);

  // State Storage and Retrieval for UI ///////////////////////////////////////

  // Save and restore the window position.
  std::wstring GetWindowPlacementKey() const;
  bool ShouldSaveWindowPlacement() const;
  void SaveWindowPlacement(const gfx::Rect& bounds, bool maximized);
  gfx::Rect GetSavedWindowBounds() const;
  bool GetSavedMaximizedState() const;

  // Gets the FavIcon of the page in the selected tab.
  SkBitmap GetCurrentPageIcon() const;

  // Gets the title of the page in the selected tab.
  string16 GetCurrentPageTitle() const;

  // Prepares a title string for display (removes embedded newlines, etc).
  static void FormatTitleForDisplay(string16* title);

  // Returns true if the frame should show a distributor logo for this Browser.
  bool ShouldShowDistributorLogo() const;

  // OnBeforeUnload handling //////////////////////////////////////////////////

  // Gives beforeunload handlers the chance to cancel the close.
  bool ShouldCloseWindow();

  // Invoked when the window containing us is closing. Performs the necessary
  // cleanup.
  void OnWindowClosing();

  // In-progress download termination handling /////////////////////////////////

  // Called when the user has decided whether to proceed or not with the browser
  // closure.  |cancel_downloads| is true if the downloads should be canceled
  // and the browser closed, false if the browser should stay open and the
  // downloads running.
  void InProgressDownloadResponse(bool cancel_downloads);

  // TabStripModel pass-thrus /////////////////////////////////////////////////

  TabStripModel* tabstrip_model() const {
    return const_cast<TabStripModel*>(&tabstrip_model_);
  }

  int tab_count() const { return tabstrip_model_.count(); }
  int selected_index() const { return tabstrip_model_.selected_index(); }
  int GetIndexOfController(const NavigationController* controller) const {
    return tabstrip_model_.GetIndexOfController(controller);
  }
  TabContents* GetTabContentsAt(int index) const {
    return tabstrip_model_.GetTabContentsAt(index);
  }
  TabContents* GetSelectedTabContents() const {
    return tabstrip_model_.GetSelectedTabContents();
  }
  void SelectTabContentsAt(int index, bool user_gesture) {
    tabstrip_model_.SelectTabContentsAt(index, user_gesture);
  }
  void CloseAllTabs() {
    tabstrip_model_.CloseAllTabs();
  }

  // Tab adding/showing functions /////////////////////////////////////////////

  // Add a new tab with the specified URL. If instance is not null, its process
  // will be used to render the tab. |force_index| is passed through to
  // TabStripModel::AddTabContents and its meaning is documented with its
  // declaration.
  TabContents* AddTabWithURL(const GURL& url,
                             const GURL& referrer,
                             PageTransition::Type transition,
                             bool foreground,
                             int index,
                             bool force_index,
                             SiteInstance* instance);

  // Add a new tab, given a NavigationController. A TabContents appropriate to
  // display the last committed entry is created and returned.
  TabContents* AddTabWithNavigationController(NavigationController* ctrl,
                                              PageTransition::Type type);

  // Add a tab with its session history restored from the SessionRestore
  // system. If select is true, the tab is selected. |tab_index| gives the index
  // to insert the tab at. |selected_navigation| is the index of the
  // TabNavigation in |navigations| to select.
  TabContents* AddRestoredTab(const std::vector<TabNavigation>& navigations,
                              int tab_index,
                              int selected_navigation,
                              bool select);
  // Creates a new tab with the already-created TabContents 'new_contents'.
  // The window for the added contents will be reparented correctly when this
  // method returns.  If |disposition| is NEW_POPUP, |pos| should hold the
  // initial position.
  void AddTabContents(TabContents* new_contents,
                      WindowOpenDisposition disposition,
                      const gfx::Rect& initial_pos,
                      bool user_gesture);
  void CloseTabContents(TabContents* contents);

  // Show a dialog with HTML content. |delegate| contains a pointer to the
  // delegate who knows how to display the dialog (which file URL and JSON
  // string input to use during initialization). |parent_window| is the window
  // that should be parent of the dialog, or NULL for the default.
  void BrowserShowHtmlDialog(HtmlDialogUIDelegate* delegate,
                             gfx::NativeWindow parent_window);

  // Called when a popup select is about to be displayed.
  void BrowserRenderWidgetShowing();

  // Notification that some of our content has changed size as
  // part of an animation.
  void ToolbarSizeChanged(bool is_animating);

  // Replaces the state of the currently selected tab with the session
  // history restored from the SessionRestore system.
  void ReplaceRestoredTab(
      const std::vector<TabNavigation>& navigations,
      int selected_navigation);

  // Returns true if a tab can be restored.
  virtual bool CanRestoreTab();

  // Show a DOMUI tab given a URL. If a tab with the same URL is already
  // visible in this browser, it becomes selected. Otherwise a new tab is
  // created.
  void ShowSingleDOMUITab(const GURL& url);

  // Assorted browser commands ////////////////////////////////////////////////

  // NOTE: Within each of the following sections, the IDs are ordered roughly by
  // how they appear in the GUI/menus (left to right, top to bottom, etc.).

  // Navigation commands
  void GoBack(WindowOpenDisposition disposition);
  void GoForward(WindowOpenDisposition disposition);
  void Reload();
  void Home(WindowOpenDisposition disposition);
  void OpenCurrentURL();
  void Go(WindowOpenDisposition disposition);
  void Stop();
  // Window management commands
  void NewWindow();
  void NewIncognitoWindow();
  void NewProfileWindowByIndex(int index);
  void CloseWindow();
  void NewTab();
  void CloseTab();
  void SelectNextTab();
  void SelectPreviousTab();
  void SelectNumberedTab(int index);
  void SelectLastTab();
  void DuplicateTab();
  void RestoreTab();
  void ConvertPopupToTabbedBrowser();
  void ToggleFullscreenMode();
  void Exit();

  // Page-related commands
  void BookmarkCurrentPage();
  void SavePage();
  void ViewSource();
  void ShowFindBar();

  // Returns true if the Browser supports the specified feature.
  virtual bool SupportsWindowFeature(WindowFeature feature) const;

// TODO(port): port these, and re-merge the two function declaration lists.
#if defined(OS_WIN)
  // Page-related commands.
  void ClosePopups();
  void Print();
#endif
  void ToggleEncodingAutoDetect();
  void OverrideEncoding(int encoding_id);

#if defined(OS_WIN)
  // Clipboard commands
  void Cut();
  void Copy();
  void CopyCurrentPageURL();
  void Paste();
#endif

  // Find-in-page
  void Find();
  void FindNext();
  void FindPrevious();

  // Zoom
  void ZoomIn();
  void ZoomReset();
  void ZoomOut();

  // Focus various bits of UI
  void FocusToolbar();
  void FocusLocationBar();
  void FocusSearch();

  // Show various bits of UI
  void OpenFile();
  void OpenCreateShortcutsDialog();
  void OpenJavaScriptConsole();
  void OpenTaskManager();
  void OpenSelectProfileDialog();
  void OpenNewProfileDialog();
  void OpenBugReportDialog();

  void ToggleBookmarkBar();

  void OpenBookmarkManager();
  void ShowHistoryTab();
  void ShowDownloadsTab();
  void OpenClearBrowsingDataDialog();
  void OpenOptionsDialog();
  void OpenKeywordEditor();
  void OpenPasswordManager();
  void OpenImportSettingsDialog();
  void OpenAboutChromeDialog();
  void OpenHelpTab();
#if defined(OS_CHROMEOS)
  void ShowControlPanel();
#endif

  virtual void UpdateDownloadShelfVisibility(bool visible);

  /////////////////////////////////////////////////////////////////////////////

  static void RegisterPrefs(PrefService* prefs);
  static void RegisterUserPrefs(PrefService* prefs);

  // Returns the Browser which contains the tab with the given
  // NavigationController, also filling in |index| (if valid) with the tab's
  // index in the tab strip.
  // Returns NULL if not found.
  // This call is O(N) in the number of tabs.
  static Browser* GetBrowserForController(
      const NavigationController* controller, int* index);


  // Helper function to create a new popup window.
  static void BuildPopupWindowHelper(TabContents* source,
                                     TabContents* new_contents,
                                     const gfx::Rect& initial_pos,
                                     Browser::Type browser_type,
                                     Profile* profile,
                                     bool honor_saved_maximized_state);

  // Calls ExecuteCommandWithDisposition with the given disposition.
  void ExecuteCommandWithDisposition(int id, WindowOpenDisposition);

  // Interface implementations ////////////////////////////////////////////////

  // Overridden from PageNavigator
  virtual void OpenURL(const GURL& url, const GURL& referrer,
                       WindowOpenDisposition disposition,
                       PageTransition::Type transition);

  // Overridden from CommandUpdater::CommandUpdaterDelegate:
  virtual void ExecuteCommand(int id);

 private:
  // Overridden from TabStripModelDelegate:
  virtual TabContents* AddBlankTab(bool foreground);
  virtual TabContents* AddBlankTabAt(int index, bool foreground);
  virtual Browser* CreateNewStripWithContents(TabContents* detached_contents,
                                              const gfx::Rect& window_bounds,
                                              const DockInfo& dock_info);
  virtual void ContinueDraggingDetachedTab(TabContents* contents,
                                           const gfx::Rect& window_bounds,
                                           const gfx::Rect& tab_bounds);
  virtual int GetDragActions() const;
  // Construct a TabContents for a given URL, profile and transition type.
  // If instance is not null, its process will be used to render the tab.
  virtual TabContents* CreateTabContentsForURL(const GURL& url,
                                               const GURL& referrer,
                                               Profile* profile,
                                               PageTransition::Type transition,
                                               bool defer_load,
                                               SiteInstance* instance) const;
  virtual bool CanDuplicateContentsAt(int index);
  virtual void DuplicateContentsAt(int index);
  virtual void CloseFrameAfterDragSession();
  virtual void CreateHistoricalTab(TabContents* contents);
  virtual bool RunUnloadListenerBeforeClosing(TabContents* contents);
  virtual bool CanCloseContentsAt(int index);

  // Overridden from TabStripModelObserver:
  virtual void TabInsertedAt(TabContents* contents,
                             int index,
                             bool foreground);
  virtual void TabClosingAt(TabContents* contents, int index);
  virtual void TabDetachedAt(TabContents* contents, int index);
  virtual void TabDeselectedAt(TabContents* contents, int index);
  virtual void TabSelectedAt(TabContents* old_contents,
                             TabContents* new_contents,
                             int index,
                             bool user_gesture);
  virtual void TabMoved(TabContents* contents,
                        int from_index,
                        int to_index);
  virtual void TabStripEmpty();

  // Overridden from TabContentsDelegate:
  virtual void OpenURLFromTab(TabContents* source,
                              const GURL& url,
                              const GURL& referrer,
                              WindowOpenDisposition disposition,
                              PageTransition::Type transition);
  virtual void NavigationStateChanged(const TabContents* source,
                                      unsigned changed_flags);
  virtual void AddNewContents(TabContents* source,
                              TabContents* new_contents,
                              WindowOpenDisposition disposition,
                              const gfx::Rect& initial_pos,
                              bool user_gesture);
  virtual void ActivateContents(TabContents* contents);
  virtual void LoadingStateChanged(TabContents* source);
  virtual void CloseContents(TabContents* source);
  virtual void MoveContents(TabContents* source, const gfx::Rect& pos);
  virtual void DetachContents(TabContents* source);
  virtual bool IsPopup(TabContents* source);
  virtual void ToolbarSizeChanged(TabContents* source, bool is_animating);
  virtual void URLStarredChanged(TabContents* source, bool starred);
  virtual void UpdateTargetURL(TabContents* source, const GURL& url);
  virtual void ContentsMouseEvent(TabContents* source, bool motion);
  virtual void ContentsZoomChange(bool zoom_in);
  virtual void TabContentsFocused(TabContents* tab_content);
  virtual bool IsApplication() const;
  virtual void ConvertContentsToApplication(TabContents* source);
  virtual bool ShouldDisplayURLField();
  virtual gfx::Rect GetRootWindowResizerRect() const;
  virtual void ShowHtmlDialog(HtmlDialogUIDelegate* delegate,
                              gfx::NativeWindow parent_window);
  virtual void BeforeUnloadFired(TabContents* source,
                                 bool proceed,
                                 bool* proceed_to_fire_unload);
  virtual void SetFocusToLocationBar();
  virtual void RenderWidgetShowing();
  virtual int GetExtraRenderViewHeight() const;
  virtual void OnStartDownload(DownloadItem* download);
  virtual void ConfirmAddSearchProvider(const TemplateURL* template_url,
                                        Profile* profile);

  // Overridden from SelectFileDialog::Listener:
  virtual void FileSelected(const FilePath& path, int index, void* params);

  // Overridden from NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Command and state updating ///////////////////////////////////////////////

  // Initialize state for all browser commands.
  void InitCommandState();

  // Update commands whose state depends on the tab's state.
  void UpdateCommandsForTabState();

  // Update commands whose state depends on whether the window is in fullscreen
  // mode.
  void UpdateCommandsForFullscreenMode(bool is_fullscreen);

  // Ask the Stop/Go button to change its icon, and update the Go and Stop
  // command states.  |is_loading| is true if the current TabContents is
  // loading.  |force| is true if the button should change its icon immediately.
  void UpdateStopGoState(bool is_loading, bool force);

  // UI update coalescing and handling ////////////////////////////////////////

  // Asks the toolbar (and as such the location bar) to update its state to
  // reflect the current tab's current URL, security state, etc.
  // If |should_restore_state| is true, we're switching (back?) to this tab and
  // should restore any previous location bar state (such as user editing) as
  // well.
  void UpdateToolbar(bool should_restore_state);

  // Adds an update to the update queue and schedules an update if necessary.
  // These are subsequently processed by ProcessPendingUIUpdates.
  // |changed_flags| is a bitfield of TabContents::INVALIDATE_* values.
  void ScheduleUIUpdate(const TabContents* source, unsigned changed_flags);

  // Processes all pending updates to the UI that have been queued by
  // ScheduleUIUpdate in scheduled_updates_.
  void ProcessPendingUIUpdates();

  // Removes all entries from scheduled_updates_ whose source is contents.
  void RemoveScheduledUpdatesFor(TabContents* contents);

  // Getters for UI ///////////////////////////////////////////////////////////

  // TODO(beng): remove, and provide AutomationProvider a better way to access
  //             the LocationBarView's edit.
  friend class AutomationProvider;

  // Returns the StatusBubble from the current toolbar. It is possible for
  // this to return NULL if called before the toolbar has initialized.
  // TODO(beng): remove this.
  StatusBubble* GetStatusBubble();

  // Session restore functions ////////////////////////////////////////////////

  // Notifies the history database of the index for all tabs whose index is
  // >= index.
  void SyncHistoryWithTabs(int index);

  // Called from AddRestoredTab and ReplaceRestoredTab to build a
  // TabContents from an incoming vector of TabNavigations.
  // Caller takes ownership of the returned TabContents.
  TabContents* BuildRestoredTab(const std::vector<TabNavigation>& navigations,
                                int selected_navigation);

  // OnBeforeUnload handling //////////////////////////////////////////////////

  typedef std::set<TabContents*> UnloadListenerSet;

  // Processes the next tab that needs it's beforeunload/unload event fired.
  void ProcessPendingTabs();

  // Whether we've completed firing all the tabs' beforeunload/unload events.
  bool HasCompletedUnloadProcessing() const;

  // Clears all the state associated with processing tabs' beforeunload/unload
  // events since the user cancelled closing the window.
  void CancelWindowClose();

  // Removes |tab| from the passed |set|.
  // Returns whether the tab was in the set in the first place.
  // TODO(beng): this method needs a better name!
  bool RemoveFromSet(UnloadListenerSet* set, TabContents* tab);

  // Cleans up state appropriately when we are trying to close the browser and
  // the tab has finished firing its unload handler. We also use this in the
  // cases where a tab crashes or hangs even if the beforeunload/unload haven't
  // successfully fired.
  void ClearUnloadState(TabContents* tab);

  // In-progress download termination handling /////////////////////////////////

  // Called when the window is closing to check if potential in-progress
  // downloads should prevent it from closing.
  // Returns true if the window can close, false otherwise.
  bool CanCloseWithInProgressDownloads();

  // Assorted utility functions ///////////////////////////////////////////////

  // Retrieve the last active tabbed browser with the same profile as the
  // receiving Browser. Creates a new Browser if none are available.
  Browser* GetOrCreateTabbedBrowser();

  // The low-level function that other OpenURL...() functions call.  This
  // determines the appropriate SiteInstance to pass to AddTabWithURL(), focuses
  // the newly created tab as needed, and does other miscellaneous housekeeping.
  void OpenURLAtIndex(TabContents* source,
                      const GURL& url,
                      const GURL& referrer,
                      WindowOpenDisposition disposition,
                      PageTransition::Type transition,
                      int index,
                      bool force_index);

  // Creates a new popup window with its own Browser object with the
  // incoming sizing information. |initial_pos|'s origin() is the
  // window origin, and its size() is the size of the content area.
  void BuildPopupWindow(TabContents* source,
                        TabContents* new_contents,
                        const gfx::Rect& initial_pos);

  // Returns what the user's home page is, or the new tab page if the home page
  // has not been set.
  GURL GetHomePage() const;

  // Shows the Find Bar, optionally selecting the next entry that matches the
  // existing search string for that Tab. |forward_direction| controls the
  // search direction.
  void FindInPage(bool find_next, bool forward_direction);

  // Closes the frame.
  // TODO(beng): figure out if we need this now that the frame itself closes
  //             after a return to the message loop.
  void CloseFrame();

  // Compute a deterministic name based on the URL. We use this pseudo name
  // as a key to store window location per application URLs.
  static std::wstring ComputeApplicationNameFromURL(const GURL& url);

  FRIEND_TEST(BrowserTest, NoTabsInPopups);

  // Create a preference dictionary for the provided application name. This is
  // done only once per application name / per session.
  static void RegisterAppPrefs(const std::wstring& app_name);

  // Data members /////////////////////////////////////////////////////////////

  NotificationRegistrar registrar_;

  // This Browser's type.
  Type type_;

  // This Browser's profile.
  Profile* profile_;

  // This Browser's window.
  BrowserWindow* window_;

  // This Browser's TabStripModel.
  TabStripModel tabstrip_model_;

  // The CommandUpdater that manages the browser window commands.
  CommandUpdater command_updater_;

  // An optional application name which is used to retrieve and save window
  // positions.
  std::wstring app_name_;

  // Unique identifier of this browser for session restore. This id is only
  // unique within the current session, and is not guaranteed to be unique
  // across sessions.
  const SessionID session_id_;

  // TODO(beng): should be combined with ToolbarModel now that this is the only
  //             implementation.
  class BrowserToolbarModel : public ToolbarModel {
  public:
    explicit BrowserToolbarModel(Browser* browser) : browser_(browser) { }
    virtual ~BrowserToolbarModel() { }

    // ToolbarModel implementation.
    virtual NavigationController* GetNavigationController();

  private:
    Browser* browser_;

    DISALLOW_COPY_AND_ASSIGN(BrowserToolbarModel);
  };

  // The model for the toolbar view.
  BrowserToolbarModel toolbar_model_;

  // UI update coalescing and handling ////////////////////////////////////////

  // Tracks invalidates to the UI, see the declaration in the .cc file.
  struct UIUpdate;
  typedef std::vector<UIUpdate> UpdateVector;

  // Lists all UI updates that are pending. We don't update things like the
  // URL or tab title right away to avoid flickering and extra painting.
  // See ScheduleUIUpdate and ProcessPendingUIUpdates.
  UpdateVector scheduled_updates_;

  // The following factory is used for chrome update coalescing.
  ScopedRunnableMethodFactory<Browser> chrome_updater_factory_;

  // OnBeforeUnload handling //////////////////////////////////////////////////

  // Tracks tabs that need there beforeunload event fired before we can
  // close the browser. Only gets populated when we try to close the browser.
  UnloadListenerSet tabs_needing_before_unload_fired_;

  // Tracks tabs that need there unload event fired before we can
  // close the browser. Only gets populated when we try to close the browser.
  UnloadListenerSet tabs_needing_unload_fired_;

  // Whether we are processing the beforeunload and unload events of each tab
  // in preparation for closing the browser.
  bool is_attempting_to_close_browser_;

  // In-progress download termination handling /////////////////////////////////

  enum CancelDownloadConfirmationState {
    NOT_PROMPTED,          // We have not asked the user.
    WAITING_FOR_RESPONSE,  // We have asked the user and have not received a
                           // reponse yet.
    RESPONSE_RECEIVED      // The user was prompted and made a decision already.
  };

  // State used to figure-out whether we should prompt the user for confirmation
  // when the browser is closed with in-progress downloads.
  CancelDownloadConfirmationState cancel_download_confirmation_state_;

  /////////////////////////////////////////////////////////////////////////////

  // Override values for the bounds of the window and its maximized state.
  // These are supplied by callers that don't want to use the default values.
  // The default values are typically loaded from local state (last session),
  // obtained from the last window of the same type, or obtained from the
  // shell shortcut's startup info.
  gfx::Rect override_bounds_;
  MaximizedState maximized_state_;

  // The following factory is used to close the frame at a later time.
  ScopedRunnableMethodFactory<Browser> method_factory_;

  // The Find Bar. This may be NULL if there is no Find Bar, and if it is
  // non-NULL, it may or may not be visible.
  scoped_ptr<FindBarController> find_bar_controller_;

  // Dialog box used for opening and saving files.
  scoped_refptr<SelectFileDialog> select_file_dialog_;

  // The browser idle task helps cleanup unused memory resources when idle.
  scoped_ptr<BrowserIdleTimer> idle_task_;

  // Keep track of the encoding auto detect pref.
  BooleanPrefMember encoding_auto_detect_;

  DISALLOW_COPY_AND_ASSIGN(Browser);
};

#endif  // CHROME_BROWSER_BROWSER_H_
