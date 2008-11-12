// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_H_
#define CHROME_BROWSER_BROWSER_H_

#include "chrome/browser/controller.h"
#include "chrome/browser/shell_dialogs.h"
#include "chrome/browser/browser_type.h"
#include "chrome/browser/session_id.h"
#include "chrome/browser/tab_contents_delegate.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/toolbar_model.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_member.h"

class BrowserIdleTimer;
class BrowserWindow;
class DebuggerWindow;
class GoButton;
class LocationBarView;
class PrefService;
class Profile;
class StatusBubble;
struct TabNavigation;
class WebContents;
class WebApp;

class Browser : public TabStripModelDelegate,
                public TabStripModelObserver,
                public TabContentsDelegate,
                public CommandHandler,
                public NotificationObserver,
                public SelectFileDialog::Listener {
 public:
  // Constructors, Creation, Showing //////////////////////////////////////////

  // Creates a new browser with the given bounds. If the bounds are empty, the
  // system will try to find a saved size from a previous session, if none
  // exists, the operating system will be allowed to size the window.
  // |type| defines the kind of browser to create.
  //
  // Creating a browser does NOT show the window. You must manually call Show()
  // to display the window.
  Browser(const gfx::Rect& initial_bounds,
          int show_command,
          Profile* profile,
          BrowserType::Type browser_type,
          const std::wstring& app_name);
  ~Browser();

  // Shows the browser window. It is initially created hidden. It will be shown
  // with the show command passed to the constructor, or possibly another state
  // if it was overridden in the preferences.
  //
  // Ideally, this function is called after everything in the window is
  // initialized so that we do not have to repaint again.
  void Show();

  // Accessors ////////////////////////////////////////////////////////////////

  BrowserType::Type type() const { return type_; }
  Profile* profile() const { return profile_; }
  BrowserWindow* window() const { return window_; }
  ToolbarModel* toolbar_model() { return &toolbar_model_; }
  const SessionID& session_id() const { return session_id_; }
  CommandController* controller() { return &controller_; }

  // Browser Creation Helpers /////////////////////////////////////////////////

  // Opens a new browser window for the specified |profile|, shown according to
  // |show_command|
  static void OpenNewBrowserWindow(Profile* profile, int show_command);

  // Opens the specified URL in a new browser window in an incognito session.
  // If there is already an existing active incognito session for the specified
  // |profile|, that session is re-used.
  static void OpenURLOffTheRecord(Profile* profile, const GURL& url);

  // Opens the a new application window for the specified WebApp.
  static void OpenWebApplication(Profile* profile,
                                 WebApp* app,
                                 int show_command);

  // Command API //////////////////////////////////////////////////////////////

  // Please fix the incestuous nest that is */controller.h and eliminate the
  // need for this retarded hack.
  bool SupportsCommand(int id) const;
  bool IsCommandEnabled(int id) const;

  // DEPRECATED DEPRECATED DEPRECATED /////////////////////////////////////////

  // Returns the HWND of the top-level system window for this Browser.
  HWND GetTopLevelHWND() const;

  // State Storage and Retrieval for UI ///////////////////////////////////////

  // Save and restore the window position.
  void SaveWindowPosition(const gfx::Rect& bounds, bool maximized);
  void RestoreWindowPosition(gfx::Rect* bounds, bool* maximized);

  // Gets the FavIcon of the page in the selected tab.
  SkBitmap GetCurrentPageIcon() const;

  // Gets the title of the page in the selected tab.
  std::wstring GetCurrentPageTitle() const;

  // Prepares a title string for display (removes embedded newlines, etc).
  static void FormatTitleForDisplay(std::wstring* title);

  // OnBeforeUnload handling //////////////////////////////////////////////////

  // Gives beforeunload handlers the chance to cancel the close.
  bool ShouldCloseWindow();

  // Invoked when the window containing us is closing. Performs the necessary
  // cleanup.
  void OnWindowClosing();

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
  NavigationController* GetSelectedNavigationController() const;
  void SelectTabContentsAt(int index, bool user_gesture) {
    tabstrip_model_.SelectTabContentsAt(index, user_gesture);
  }
  TabContents* AddBlankTab(bool foreground) {
    return tabstrip_model_.AddBlankTab(foreground);
  }
  void CloseAllTabs() {
    tabstrip_model_.CloseAllTabs();
  }

  // Tab adding/showing functions /////////////////////////////////////////////

  // Add a new tab with the specified URL. If instance is not null, its process
  // will be used to render the tab.
  TabContents* AddTabWithURL(
      const GURL& url, const GURL& referrer,
      PageTransition::Type transition, bool foreground,
      SiteInstance* instance);

  // Add a new application tab for the specified URL. If lazy is true, the tab
  // won't be selected. Further, the initial web page load will only take place
  // when the tab is first selected.
  TabContents* AddWebApplicationTab(Profile* profile,
                                    WebApp* web_app,
                                    bool lazy);

  // Add a new tab, given a NavigationController. A TabContents appropriate to
  // display the last committed entry is created and returned.
  TabContents* AddTabWithNavigationController(NavigationController* ctrl,
                                              PageTransition::Type type);

  // Add a tab with its session history restored from the SessionRestore
  // system. If select is true, the tab is selected. Returns the created
  // NavigationController. |tab_index| gives the index to insert the tab at.
  // |selected_navigation| is the index of the TabNavigation in |navigations|
  // to select.
  NavigationController* AddRestoredTab(
      const std::vector<TabNavigation>& navigations,
      int tab_index,
      int selected_navigation,
      bool select);

  // Replaces the state of the currently selected tab with the session
  // history restored from the SessionRestore system.
  void ReplaceRestoredTab(
      const std::vector<TabNavigation>& navigations,
      int selected_navigation);

  // Show a native UI tab given a URL. If a tab with the same URL is already
  // visible in this browser, it becomes selected. Otherwise a new tab is
  // created.
  void ShowNativeUITab(const GURL& url);

  // Assorted browser commands ////////////////////////////////////////////////

  // Navigation Commands
  void GoBack();
  void GoForward();
  void Reload();
  void Stop();
  void Home();

  // Sets focus on the location bar's text field - public because 
  void FocusLocationBar();

  // Adds a Bookmark for the currently selected tab.
  void BookmarkCurrentPage();

  // Open the debugger shell.
  void OpenDebuggerWindow();

  // Opens the FindInPage window for the currently open tab.
  void OpenFindInPageWindow();

  // Advance the find selection by one. Direction is either forward or
  // backwards depending on parameter passed in.
  void AdvanceFindSelection(bool forward_direction);

  // Convert the receiving Browser to a normal browser window. This is used to
  // convert a popup window into a normal browser window. The receiver's type
  // must be BROWSER.
  void ConvertToTabbedBrowser();

  // Opens the Keyword Editor
  void OpenKeywordEditor();

  // Opens the Clear Browsing Data dialog.
  void OpenClearBrowsingDataDialog();

  // Opens the Import settings dialog.
  void OpenImportSettingsDialog();

  // Opens the Bug Report dialog.
  void OpenBugReportDialog();

  // Copy the current page URL to the clipboard.
  void CopyCurrentURLToClipBoard();

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

  // Interface implementations ////////////////////////////////////////////////

  // Overridden from CommandHandler:
  virtual bool GetContextualLabel(int id, std::wstring* out) const {
    return false;
  }
  virtual void ExecuteCommand(int id);

  // Overridden from TabStripModelDelegate:
  virtual void CreateNewStripWithContents(TabContents* detached_contents,
                                          const gfx::Point& drop_point);
  virtual int GetDragActions() const;
  // Construct a TabContents for a given URL, profile and transition type.
  // If instance is not null, its process will be used to render the tab.
  // TODO(beng): remove this from TabStripDelegate, it's only used by
  //             TabStripModel::AddBlankTab*, which should really live here
  //             on Browser.
  virtual TabContents* CreateTabContentsForURL(
      const GURL& url,
      const GURL& referrer,
      Profile* profile,
      PageTransition::Type transition,
      bool defer_load,
      SiteInstance* instance) const;
  virtual bool CanDuplicateContentsAt(int index);
  virtual void DuplicateContentsAt(int index);
  virtual void ValidateLoadingAnimations();
  virtual void CloseFrameAfterDragSession();

  // Overridden from TabStripModelObserver:
  virtual void TabInsertedAt(TabContents* contents,
                             int index,
                             bool foreground);
  virtual void TabClosingAt(TabContents* contents, int index);
  virtual void TabDetachedAt(TabContents* contents, int index);
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
                             const GURL& url, const GURL& referrer,
                             WindowOpenDisposition disposition,
                             PageTransition::Type transition);
  virtual void NavigationStateChanged(const TabContents* source,
                                      unsigned changed_flags);
  virtual void ReplaceContents(TabContents* source, TabContents* new_contents);
  virtual void AddNewContents(TabContents* source,
                              TabContents* new_contents,
                              WindowOpenDisposition disposition,
                              const gfx::Rect& initial_pos,
                              bool user_gesture);
  virtual void ActivateContents(TabContents* contents);
  virtual void LoadingStateChanged(TabContents* source);
  virtual void CloseContents(TabContents* source);
  virtual void MoveContents(TabContents* source, const gfx::Rect& pos);
  virtual bool IsPopup(TabContents* source);
  virtual void ToolbarSizeChanged(TabContents* source, bool is_animating);
  virtual void URLStarredChanged(TabContents* source, bool starred);

  virtual void ContentsMouseEvent(TabContents* source, UINT message);
  virtual void UpdateTargetURL(TabContents* source, const GURL& url);

  virtual void ContentsZoomChange(bool zoom_in);
  virtual bool IsApplication() const;
  virtual void ConvertContentsToApplication(TabContents* source);
  virtual void ContentsStateChanged(TabContents* source);
  virtual bool ShouldDisplayURLField();
  virtual void BeforeUnloadFired(TabContents* source,
                                 bool proceed, 
                                 bool* proceed_to_fire_unload);
  virtual void ShowHtmlDialog(HtmlDialogContentsDelegate* delegate,
                              HWND parent_hwnd);

  // Overridden from SelectFileDialog::Listener:
  virtual void FileSelected(const std::wstring& path, void* params);

  // Overridden from NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  // Command and state updating ///////////////////////////////////////////////

  // Initialize state for all browser commands.
  void InitCommandState();

  // Update commands that drive the NavigationController to reflect changes in
  // the NavigationController's state (Back, Forward, etc).
  void UpdateNavigationCommands();

  // Change the "starred" button display to starred/unstarred.
  // TODO(evanm): migrate this to the commands framework.
  void SetStarredButtonToggled(bool starred);

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

  // Getters for the location bar and go button.
  LocationBarView* GetLocationBarView() const;
  GoButton* GetGoButton();

  // Returns the StatusBubble from the current toolbar. It is possible for
  // this to return NULL if called before the toolbar has initialized.
  // TODO(beng): remove this.
  StatusBubble* GetStatusBubble();

  // Session restore functions ////////////////////////////////////////////////

  // Notifies the history database of the index for all tabs whose index is
  // >= index.
  void SyncHistoryWithTabs(int index);

  // Called from AddRestoredTab and ReplaceRestoredTab to build a
  // NavigationController from an incoming vector of TabNavigations.
  // Caller takes ownership of the returned NavigationController.
  NavigationController* BuildRestoredNavigationController(
      const std::vector<TabNavigation>& navigations,
      int selected_navigation);

  // OnBeforeUnload handling //////////////////////////////////////////////////

  typedef std::vector<TabContents*> UnloadListenerVector;

  // Processes the next tab that needs it's beforeunload/unload event fired.
  void ProcessPendingTabs();

  // Whether we've completed firing all the tabs' beforeunload/unload events.
  bool HasCompletedUnloadProcessing();

  // Clears all the state associated with processing tabs' beforeunload/unload
  // events since the user cancelled closing the window.
  void CancelWindowClose();

  // Removes the tab from the associated vector. Returns whether the tab
  // was in the vector in the first place.
  // TODO(beng): this method needs a better name!
  bool RemoveFromVector(UnloadListenerVector* vector, TabContents* tab);

  // Cleans up state appropriately when we are trying to close the browser and
  // the tab has finished firing it's unload handler. We also use this in the 
  // cases where a tab crashes or hangs even if the beforeunload/unload haven't
  // successfully fired.
  void ClearUnloadState(TabContents* tab);

  // Assorted utility functions ///////////////////////////////////////////////

  // Retrieve the last active tabbed browser with the same profile as the
  // receiving Browser. Creates a new Browser if none are available.
  Browser* GetOrCreateTabbedBrowser();

  // Creates a new popup window with its own Browser object with the
  // incoming sizing information. |initial_pos|'s origin() is the
  // window origin, and its size() is the size of the content area.
  void BuildPopupWindow(TabContents* source,
                        TabContents* new_contents,
                        const gfx::Rect& initial_pos);

  // Returns what the user's home page is, or the new tab page if the home page
  // has not been set.
  GURL GetHomePage();

  // Closes the frame.
  // TODO(beng): figure out if we need this now that the frame itself closes
  //             after a return to the message loop.
  void CloseFrame();

  // Compute a deterministic name based on the URL. We use this pseudo name
  // as a key to store window location per application URLs.
  static std::wstring ComputeApplicationNameFromURL(const GURL& url);

  // Create a preference dictionary for the provided application name. This is
  // done only once per application name / per session.
  static void RegisterAppPrefs(const std::wstring& app_name);

  // Data members /////////////////////////////////////////////////////////////

  // This Browser's type.
  BrowserType::Type type_;

  // This Browser's profile.
  Profile* profile_;

  // This Browser's window.
  BrowserWindow* window_;

  // Controls how the window will appear when Show() is called. This is one
  // of the SW_* constants passed to ShowWindow, and will be initialized in the
  // constructor.
  //
  // After the first call to Show() succeeds, this is set to -1, indicating that
  // subsequent calls to Show() should be ignored.
  // TODO(beng): This should be removed (http://crbug.com/3557) and put into
  //             BrowserView, or some more likely place.
  int initial_show_command_;

  // This Browser's TabStripModel.
  TabStripModel tabstrip_model_;

  // The Controller that updates all browser commands.
  CommandController controller_;

  // An optional application name which is used to retrieve and save window
  // positions.
  std::wstring app_name_;

  // Unique identifier of this browser for session restore. This id is only
  // unique within the current session, and is not guaranteed to be unique
  // across sessions.
  SessionID session_id_;

  // TODO(beng): should be combined with ToolbarModel now that this is the only
  //             implementation.
  class BrowserToolbarModel : public ToolbarModel {
  public:
    explicit BrowserToolbarModel(Browser* browser) : browser_(browser) { }
    virtual ~BrowserToolbarModel() { }

    // ToolbarModel implementation.
    virtual NavigationController* GetNavigationController() {
      return browser_->GetSelectedNavigationController();
    }

  private:
    Browser* browser_;

    DISALLOW_EVIL_CONSTRUCTORS(BrowserToolbarModel);
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
  UnloadListenerVector tabs_needing_before_unload_fired_;

  // Tracks tabs that need there unload event fired before we can
  // close the browser. Only gets populated when we try to close the browser.
  UnloadListenerVector tabs_needing_unload_fired_;

  // Whether we are processing the beforeunload and unload events of each tab
  // in preparation for closing the browser.
  bool is_attempting_to_close_browser_;

  /////////////////////////////////////////////////////////////////////////////

  // The following factory is used to close the frame at a later time.
  ScopedRunnableMethodFactory<Browser> method_factory_;

  // Debugger Window, created lazily
  scoped_refptr<DebuggerWindow> debugger_window_;

  // Dialog box used for opening and saving files.
  scoped_refptr<SelectFileDialog> select_file_dialog_;

  // The browser idle task helps cleanup unused memory resources when idle.
  scoped_ptr<BrowserIdleTimer> idle_task_;

  // Keep track of the encoding auto detect pref.
  BooleanPrefMember encoding_auto_detect_;

  DISALLOW_COPY_AND_ASSIGN(Browser);
};

#endif  // CHROME_BROWSER_BROWSER_H_
