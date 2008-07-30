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

#ifndef CHROME_BROWSER_BROWSER_H__
#define CHROME_BROWSER_BROWSER_H__

#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/browser_type.h"
#include "chrome/browser/hang_monitor/hung_plugin_action.h"
#include "chrome/browser/hang_monitor/hung_window_detector.h"
#include "chrome/browser/render_process_host.h"
#include "chrome/browser/shell_dialogs.h"
#include "chrome/browser/debugger/debugger_window.h"
#include "chrome/browser/tab_contents_delegate.h"
#include "chrome/browser/tab_contents_type.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/tabs/tab.h"
#include "chrome/browser/toolbar_model.h"
#include "chrome/browser/views/html_dialog_view.h"
#include "chrome/browser/views/toolbar_view.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_member.h"

class BookmarkBarView;
class BrowserWindow;
class GoButton;
class LocationBarView;
class PrefService;
class Profile;
class StatusBubble;
class WebContents;
class BrowserIdleTask;
class WebApp;

class Browser : public TabStripModelDelegate,
                public TabStripModelObserver,
                public TabContentsDelegate,
                public CommandHandler,
                public NotificationObserver,
                public SelectFileDialog::Listener {
 public:
  // TODO(beng): (Cleanup) This is a hack. Right now the |initial_bounds|
  // parameter to Browser's ctor specifies the size of the frame, not the size
  // of the contents that will be displayed within it. So this flag exists,
  // which can be passed instead of a typical value for |show_command| that
  // tells the Browser to create its window, and then use the |initial_bounds|
  // parameter as the size of the contents, resizing the frame to fit. See
  // SizeToContents method on chrome_frame.h
  enum {
    SIZE_TO_CONTENTS = 9999
  };

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
  void Show() { ShowAndFit(false); }

  // Like Show, but the window is optionally resized and moved to be on the
  // default screen.
  void ShowAndFit(bool resize_to_fit);

  // Returns the Browser which contains the tab with the given
  // NavigationController, also filling in |index| (if valid) with the tab's
  // index in the tab strip.
  // Returns NULL if not found.
  // This call is O(N) in the number of tabs.
  static Browser* GetBrowserForController(
      const NavigationController* controller, int* index);

  static void OpenNewBrowserWindow(Profile* profile, int show_command);

  static void RegisterPrefs(PrefService* prefs);
  static void RegisterUserPrefs(PrefService* prefs);

  // Initialize the receiver with the provided bounds which
  // is in the screen coordinate system.
  void InitWithBounds(CRect* bounds, int show_command);

  void GoBack();
  void GoForward();
  void Stop();
  void Reload();
  void Home();

  // "Stars" or (book)marks the contents of the current tab.
  void StarCurrentTabContents();

  // Opens the FindInPage window for the currently open tab.
  void OpenFindInPageWindow();
  // Becomes the parent window of the Find window of the specified tab. This is
  // useful, for example, when tabs are dragged out of (or in to) the tab strip
  // to make sure the Find window shows up in the right Browser window.
  void AdoptFindWindow(TabContents* tab_contents);

  // debugger shell
  void OpenDebuggerWindow();

  // Advance the find selection by one. Direction is either forward or backwards
  // depending on parameter passed in. If selection cannot be advanced (for
  // example because no search has been issued, then the function returns false
  // and caller can call OpenFindInPageWindow to show the search window.
  bool AdvanceFindSelection(bool forward_direction);

  Profile* profile() const { return profile_; }

  BrowserWindow* window() const { return window_; }

  ToolbarModel* toolbar_model() { return &toolbar_model_; }

  // Returns the HWND of the top-level system window for this Browser.
  HWND GetTopLevelHWND() const;

  // Update commands that drive the NavigationController to reflect changes in
  // the NavigationController's state (Back, Forward, etc).
  void UpdateNavigationCommands();

  // CommandHandler interface method implementation
  bool GetContextualLabel(int id, std::wstring* out) const;
  void ExecuteCommand(int id);

  // Please fix the incestuous nest that is */controller.h and eliminate the
  // need for this retarded hack.
  bool SupportsCommand(int id) const;
  bool IsCommandEnabled(int id) const;

  // Sets focus on the location bar's text field.
  void FocusLocationBar();

  // Notification that some of our content has animated. If the source
  // is the current tab, this invokes the same method on the frame.
  void ToolbarSizeChanged(TabContents* source, bool is_animating);

  // Move the window to the front.
  void MoveToFront(bool should_activate);

  // Unique identifier for this window; used for session restore.
  const SessionID& session_id() const { return session_id_; }

  // Executes a Windows WM_APPCOMMAND command id. This function translates a
  // button-specific identifier to an id understood by our controller.
  bool ExecuteWindowsAppCommand(int app_command_id);

  // Gives beforeunload handlers the chance to cancel the close.
  bool ShouldCloseWindow();

  // Tells us that we've finished firing this tab's beforeunload event.
  // The proceed bool tells us whether the user chose to proceed closing the
  // tab.
  void BeforeUnloadFired(TabContents* source, bool proceed);

  // Tells us that we've finished firing this tab's unload event.
  void UnloadFired(TabContents* source);

  // Invoked when the window containing us is closing. Performs the necessary
  // cleanup.
  void OnWindowClosing();

  // TabStripModel pass-thrus //////////////////////////////////////////////////

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

  // Tab Creation functions ////////////////////////////////////////////////////

  // Add a new tab with the specified URL. If instance is not null, its process
  // will be used to render the tab.
  TabContents* AddTabWithURL(
      const GURL& url, PageTransition::Type transition, bool foreground,
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
  // NavigationController.
  NavigationController* AddRestoredTab(
      const std::vector<TabNavigation>& navigations,
      int selected_navigation,
      bool select);

  // Replaces the state of the currently selected tab with the session
  // history restored from the SessionRestore system.
  void ReplaceRestoredTab(
      const std::vector<TabNavigation>& navigations,
      int selected_navigation);

  // Overridden from TabStripDelegate:
  virtual void CreateNewStripWithContents(TabContents* detached_contents,
                                          const gfx::Point& drop_point);
  virtual int GetDragActions() const;
  // Construct a TabContents for a given URL, profile and transition type.
  // If instance is not null, its process will be used to render the tab.
  virtual TabContents* CreateTabContentsForURL(
      const GURL& url,
      Profile* profile,
      PageTransition::Type transition,
      bool defer_load,
      SiteInstance* instance) const;
  virtual void ShowApplicationMenu(const gfx::Point p);
  virtual bool CanDuplicateContentsAt(int index);
  virtual void DuplicateContentsAt(int index);
  virtual void ValidateLoadingAnimations();
  virtual void CloseFrameAfterDragSession();

  // Overridden from TabStripObserver:
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
                             const GURL& url,
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
  virtual void StartDraggingDetachedContents(TabContents* source,
                                             TabContents* new_contents,
                                             const gfx::Rect& contents_bounds,
                                             const gfx::Point& mouse_pt,
                                             int frame_component);
  virtual void ActivateContents(TabContents* contents);
  virtual void LoadingStateChanged(TabContents* source);
  virtual void CloseContents(TabContents* source);
  virtual void MoveContents(TabContents* source, const gfx::Rect& pos);
  virtual bool IsPopup(TabContents* source);
  virtual void URLStarredChanged(TabContents* source, bool starred);

  virtual void WindowMoved();
  virtual void ContentsMouseEvent(TabContents* source, UINT message);
  virtual void UpdateTargetURL(TabContents* source, const GURL& url);

  virtual void ContentsZoomChange(bool zoom_in);
  virtual bool IsApplication() const;
  virtual void ConvertContentsToApplication(TabContents* source);
  virtual void CrashedStateChanged(TabContents* source);
  virtual bool ShouldDisplayURLField();

  // Return this browser type.
  BrowserType::Type GetType() const;

  // Invoke the menu we use for application and popup windows at the provided
  // point and for the provided hwnd.
  void RunSimpleFrameMenu(const CPoint& pt, HWND hwnd);

  // Returns the bookmark bar view, creating if NULL.
  BookmarkBarView* GetBookmarkBarView();

  // Show some native UI given a URL. If a tab with the same URL is already
  // visible in this browser, it becomes selected. Otherwise a new tab is
  // created.
  void ShowNativeUI(const GURL& url);

  // Show a dialog with HTML content. |delegate| contains a pointer to the
  // delegate who knows how to display the dialog (which file URL and JSON
  // string input to use during initialization). |parent_hwnd| is the window
  // that should be the parent of this dialog, or NULL for this browser's top
  // level hwnd.
  // TODO(beng): (Cleanup) this really shouldn't live here. It's not
  //             necessarily browser-related (e.g. can be called from options
  //             dialog).
  void ShowHtmlDialog(HtmlDialogContentsDelegate* delegate, HWND parent_hwnd);

  // Overridden from SelectFileDialog::Listener:
  virtual void FileSelected(const std::wstring& path, void* params);

  // Start an off the record session. If a window containing an off the record
  // tab for the current profile exists, create a new off the record tab in that
  // window. Otherwise, create a new window with an off the record tab.
  static void OpenURLOffTheRecord(Profile* p, const GURL& url);

  // Computes a title suitable for popups without a URL field.
  static std::wstring ComputePopupTitle(const GURL& url,
                                        const std::wstring& title);

  // Compute a deterministic name based on the URL. We use this pseudo name
  // as a key to store window location per application URLs.
  static std::wstring ComputeApplicationNameFromURL(const GURL& url);

  // Start a web application.
  static void OpenWebApplication(Profile* profile,
                                 WebApp* app,
                                 int show_command);

  // Return this browser's controller.
  CommandController* controller() { return &controller_; }

  // Returns the location bar view for this browser.
  LocationBarView* GetLocationBarView() const;

  void ConvertTabToApplication(TabContents* contents);

 private:
  friend class XPFrame;
  friend class VistaFrame;
  friend class SimpleFrame;

  // Tracks invalidates to the UI, see the declaration in the .cc file.
  struct UIUpdate;
  typedef std::vector<UIUpdate> UpdateVector;

  typedef std::vector<TabContents*> UnloadListenerVector;

  Browser();

  // Closes the frame.
  void CloseFrame();

  // Invoked by the frame. Return the toolbar for this browser.
  ChromeViews::View* GetToolbar();

  // Returns the root view for this browser.
  ChromeViews::RootView* GetRootView() const;

  // Called when this window gains or loses window-manager-level activation.
  // is_active is whether or not the Window is now active.
  void WindowActivationChanged(bool is_active);

  // Initialize state for all browser commands.
  void InitCommandState();

  // Change the "starred" button display to starred/unstarred.
  // TODO(evanm): migrate this to the commands framework.
  void SetStarredButtonToggled(bool starred);

  GoButton* GetGoButton();

  // Returns the StatusBubble from the current toolbar. It is possible for
  // this to return NULL if called before the toolbar has initialized.
  StatusBubble* GetStatusBubble();

  // Syncs the window title with current_tab_.  This may be necessary because
  // current_tab_'s title changed, or because current_tab_ itself has
  // changed.
  void SyncWindowTitle();

  // Saves the location of the window to the history database.
  void SaveWindowPlacementToDatabase();
  // Window placement memory across sessions.
  void SaveWindowPlacement();

  // Notifies the history database of the index for all tabs whose index is
  // >= index.
  void SyncHistoryWithTabs(int index);

  // Notification service callback.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // The Controller that updates all browser commands.
  CommandController controller_;

  // Asks the toolbar (and as such the location bar) to update its state to
  // reflect the current tab's current URL, security state, etc.
  // If |should_restore_state| is true, we're switching (back?) to this tab and
  // should restore any previous location bar state (such as user editing) as
  // well.
  void UpdateToolBar(bool should_restore_state);

  // Adds an update to the update queue and schedules an update if necessary.
  // These are subsequently processed by ProcessPendingUIUpdates.
  // |changed_flags| is a bitfield of TabContents::INVALIDATE_* values.
  void ScheduleUIUpdate(const TabContents* source,
                        unsigned changed_flags);

  // Processes all pending updates to the UI that have been queued by
  // ScheduleUIUpdate in scheduled_updates_.
  void ProcessPendingUIUpdates();

  // Update the current page title
  void UpdateTitle();

  // Opens the Keyword Editor
  void OpenKeywordEditor();

  // Opens the Clear Browsing Data dialog.
  void OpenClearBrowsingDataDialog();

  // Opens the Import settings dialog.
  void OpenImportSettingsDialog();

  // Opens the Bug Report dialog.
  void OpenBugReportDialog();

  // Removes the InfoBar and download shelf for the specified TabContents, if
  // they are presently attached.
  void RemoveShelvesForTabContents(TabContents* contents);

  // Copy the current page URL to the clipboard.
  void CopyCurrentURLToClipBoard();

  // Initializes the hang monitor.
  void InitHangMonitor();

  // Retrieve the last active tabbed browser with the same profile as the
  // receiving Browser. Creates a new Browser if none are available.
  Browser* GetOrCreateTabbedBrowser();

  // Removes all entries from scheduled_updates_ whose source is contents.
  void RemoveScheduledUpdatesFor(TabContents* contents);

  // Called from AddRestoredTab and ReplaceRestoredTab to build a
  // NavigationController from an incoming vector of TabNavigations.
  // Caller takes ownership of the returned NavigationController.
  NavigationController* BuildRestoredNavigationController(
      const std::vector<TabNavigation>& navigations,
      int selected_navigation);

  // Convert the receiving Browser to a normal browser window. This is used to
  // convert a popup window into a normal browser window. The receiver's type
  // must be BROWSER.
  void ConvertToTabbedBrowser();

  // Create a preference dictionary for the provided application name. This is
  // done only once per application name / per session.
  static void RegisterAppPrefs(const std::wstring& app_name);

  // Creates a new popup window with its own Browser object with the
  // incoming sizing information. |initial_pos|'s origin() is the
  // window origin, and its size() is the size of the content area.
  void BuildPopupWindow(TabContents* source,
                        TabContents* new_contents,
                        const gfx::Rect& initial_pos);

  // Processes the next tab that needs it's beforeunload event fired before
  // we can proceed with closing the browser.
  void ProcessPendingBeforeUnloadTabs();

  // Processes the next tab that needs it's unload event fired before we can
  // proceed with closing the browser.
  void ProcessPendingUnloadTabs();

  // Cleans up state appropriately when we are trying to close the browser
  // and a tab crashes in it's beforeunload/unload handler.
  void ClearUnloadStateOnCrash(TabContents* tab);

  // The frame
  BrowserWindow* window_;

  // Controls how the window will appear when Show() is called. This is one
  // of the SW_* constants passed to ShowWindow, and will be initialized in the
  // constructor.
  //
  // After the first call to Show() succeeds, this is set to -1, indicating that
  // subsequent calls to Show() should be ignored.
  int initial_show_command_;

  // The toolbar view.
  BrowserToolbarView toolbar_;

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

  TabStripModel tabstrip_model_;

  Profile* profile_;

  // Tracks tabs that need there beforeunload event fired before we can
  // close the browser. Only gets populated when we try to close the browser.
  UnloadListenerVector tabs_needing_before_unload_fired_;

  // Tracks tabs that need there unload event fired before we can
  // close the browser. Only gets populated when we try to close the browser.
  UnloadListenerVector tabs_needing_unload_fired_;

  // Whether we already handled the OnStripEmpty event - it can be called
  // multiple times.
  bool handled_strip_empty_;

  // Whether we are processing the beforeunload and unload events of each tab
  // in preparation for closing the browser.
  bool is_processing_tab_unload_events_;

  // The following factory is used for chrome update coalescing.
  ScopedRunnableMethodFactory<Browser> chrome_updater_factory_;

  // The following factory is used to close the frame at a later time.
  ScopedRunnableMethodFactory<Browser> method_factory_;

  // This object is used to perform periodic actions in a worker
  // thread. It is currently used to monitor hung plugin windows.
  WorkerThreadTicker ticker_;

  // This object is initialized with the frame window HWND. This
  // object is also passed as a tick handler with the ticker_ object.
  // It is used to periodically monitor for hung plugin windows
  HungWindowDetector hung_window_detector_;

  // This object is invoked by hung_window_detector_ when it detects a hung
  // plugin window.
  HungPluginAction hung_plugin_action_;

  // This browser type.
  BrowserType::Type type_;

  // The bookmark bar. This is lazily created.
  scoped_ptr<BookmarkBarView> bookmark_bar_view_;

  // Lists all UI updates that are pending. We don't update things like the
  // URL or tab title right away to avoid flickering and extra painting.
  // See ScheduleUIUpdate and ProcessPendingUIUpdates.
  UpdateVector scheduled_updates_;

  // An optional application name which is used to retrieve and save window
  // positions.
  std::wstring app_name_;

  // Unique identifier of this browser for session restore. This id is only
  // unique within the current session, and is not guaranteed to be unique
  // across sessions.
  SessionID session_id_;

  // Debugger Window, created lazily
  scoped_refptr<DebuggerWindow> debugger_window_;

  // Dialog box used for opening and saving files.
  scoped_refptr<SelectFileDialog> select_file_dialog_;

  // The browser idle task helps cleanup unused memory resources when idle.
  scoped_ptr<BrowserIdleTask> idle_task_;

  // Keep track of the encoding auto detect pref.
  BooleanPrefMember encoding_auto_detect_;

  DISALLOW_EVIL_CONSTRUCTORS(Browser);
};

#endif  // CHROME_BROWSER_BROWSER_H__
