// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSION_SERVICE_H__
#define CHROME_BROWSER_SESSION_SERVICE_H__

#include <map>

#include "base/basictypes.h"
#include "base/gfx/rect.h"
#include "base/task.h"
#include "base/time.h"
#include "chrome/browser/browser_type.h"
#include "chrome/browser/cancelable_request.h"
#include "chrome/browser/session_id.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/page_transition_types.h"
#include "chrome/common/stl_util-inl.h"
#include "googleurl/src/gurl.h"

class Browser;
class NavigationController;
class NavigationEntry;
class Profile;
class TabContents;
class SessionBackend;
class SessionCommand;

namespace base {
class Thread;
}

// TabNavigation  ------------------------------------------------------------

// TabNavigation corresponds to a NavigationEntry.

struct TabNavigation {
  friend class SessionService;

  enum TypeMask {
    HAS_POST_DATA = 1
  };

  TabNavigation() : transition(PageTransition::TYPED), type_mask(0), index(-1) {
  }
  TabNavigation(int index,
                const GURL& url,
                const GURL& referrer,
                const std::wstring& title,
                const std::string& state,
                PageTransition::Type transition)
      : url(url),
        referrer(referrer),
        title(title),
        state(state),
        transition(transition),
        type_mask(0),
        index(index) {}


  GURL url;
  GURL referrer;

  // The title of the page.
  std::wstring title;
  std::string state;
  PageTransition::Type transition;

  // A mask used for arbitrary boolean values needed to represent a
  // NavigationEntry. Currently only contains HAS_POST_DATA or 0.
  int type_mask;

 private:
  // The index in the NavigationController. If this is -1, it means this
  // TabNavigation is bogus.
  //
  // This is used when determining the selected TabNavigation and only useful
  // by SessionService.
  int index;
};

// SessionTab ----------------------------------------------------------------

// SessionTab corresponds to a NavigationController.

struct SessionTab {
  SessionTab() : tab_visual_index(-1), current_navigation_index(-1) { }

  // Unique id of the window.
  SessionID window_id;

  // Unique if of the tab.
  SessionID tab_id;

  // Visual index of the tab within its window. There may be gaps in these
  // values.
  //
  // NOTE: this is really only useful for the SessionService during
  // restore, others can likely ignore this and use the order of the
  // tabs in SessionWindow.tabs.
  int tab_visual_index;

  // Identifies the index of the current navigation in navigations. For
  // example, if this is 2 it means the current navigation is navigations[2].
  //
  // NOTE: when the service is creating SessionTabs, initially this
  // corresponds to TabNavigation.index, not the index in navigations. When done
  // creating though, this is set to the index in navigations.
  int current_navigation_index;

  std::vector<TabNavigation> navigations;

 private:
  DISALLOW_EVIL_CONSTRUCTORS(SessionTab);
};

// SessionWindow -------------------------------------------------------------

// Describes a saved window.

struct SessionWindow {
  SessionWindow()
      : selected_tab_index(-1),
        type(BrowserType::TABBED_BROWSER),
        is_constrained(true),
        is_maximized(false) {}
  ~SessionWindow() { STLDeleteElements(&tabs); }

  // Identifier of the window.
  SessionID window_id;

  // Bounds of the window.
  gfx::Rect bounds;

  // Index of the selected tab in tabs; -1 if no tab is selected. After restore
  // this value is guaranteed to be a valid index into tabs.
  //
  // NOTE: when the service is creating SessionWindows, initially this
  // corresponds to SessionTab.tab_visual_index, not the index in
  // tabs. When done creating though, this is set to the index in
  // tabs.
  int selected_tab_index;

  // Type of the browser. Currently we only store browsers of type
  // TABBED_BROWSER and BROWSER.
  BrowserType::Type type;

  // If true, the window is constrained.
  //
  // Currently SessionService prunes all constrained windows so that session
  // restore does not attempt to restore them.
  bool is_constrained;

  // The tabs, ordered by visual order.
  std::vector<SessionTab*> tabs;

  // Is the window maximized?
  bool is_maximized;

 private:
  DISALLOW_EVIL_CONSTRUCTORS(SessionWindow);
};

// SessionService ------------------------------------------------------------

// SessionService is responsible for maintaining the state of open windows
// and tabs so that they can be restored at a later date. The state of the
// currently open browsers is referred to as the current session.
//
// SessionService supports restoring from two distinct points (or sessions):
// . The previous or last session. The previous session typically corresponds
//   to the last run of the browser, but not always. For example, if the user
//   has a tabbed browser and app window running, closes the tabbed browser,
//   then creates a new tabbed browser the current session is made the last
//   session and the current session reset. This is done to provide the
//   illusion that app windows run in separate processes.
// . A user defined point. That is, any time CreateSavedSession is invoked
//   the save session is reset from the current state of the browser.
//
// Additionally the current session can be made the 'last' session at any point
// by way of MoveCurrentSessionToLastSession. This may be done at certain points
// during the browser that are viewed as changing the 
//
// SessionService itself maintains a set of SessionCommands that allow
// SessionService to rebuild the open state of the browser (as
// SessionWindow, SessionTab and TabNavigation). The commands are periodically
// flushed to SessionBackend and written to a file. Every so often
// SessionService rebuilds the contents of the file from the open state
// of the browser.

class SessionService : public CancelableRequestProvider,
                       public NotificationObserver,
                       public base::RefCountedThreadSafe<SessionService> {
  friend class SessionServiceTestHelper;
 public:
  // Creates a SessionService for the specified profile.
  explicit SessionService(Profile* profile);
  // For testing.
  explicit SessionService(const std::wstring& save_path);

  ~SessionService();

  // Resets the contents of the file from the current state of all open
  // browsers whose profile matches our profile.
  void ResetFromCurrentBrowsers();

  // Moves the current session to the last session. This is useful when a
  // checkpoint occurs, such as when the user launches the app and no tabbed
  // browsers are running.
  void MoveCurrentSessionToLastSession();

  // Associates a tab with a window.
  void SetTabWindow(const SessionID& window_id,
                    const SessionID& tab_id);

  // Sets the bounds of a window.
  void SetWindowBounds(const SessionID& window_id,
                       const gfx::Rect& bounds,
                       bool is_maximized);

  // Sets the visual index of the tab in its parent window.
  void SetTabIndexInWindow(const SessionID& window_id,
                           const SessionID& tab_id,
                           int new_index);

  // Notification that a tab has been closed.
  //
  // Note: this is invoked from the NavigationController's destructor, which is
  // after the actual tab has been removed.
  void TabClosed(const SessionID& window_id, const SessionID& tab_id);

  // Notification the window is about to close.
  void WindowClosing(const SessionID& window_id);

  // Notification a window has finished closing.
  void WindowClosed(const SessionID& window_id);

  // Sets the type of window. In order for the contents of a window to be
  // tracked SetWindowType must be invoked with a type we track
  // (should_track_changes_for_browser_type returns true).
  void SetWindowType(const SessionID& window_id, BrowserType::Type type);

  // Invoked when the NavigationController has removed entries from the back of
  // the list. |count| gives the number of entries in the navigation controller.
  void TabNavigationPathPrunedFromBack(const SessionID& window_id,
                                       const SessionID& tab_id,
                                       int count);

  // Invoked when the NavigationController has removed entries from the front of
  // the list. |count| gives the number of entries that were removed.
  void TabNavigationPathPrunedFromFront(const SessionID& window_id,
                                        const SessionID& tab_id,
                                        int count);

  // Updates the navigation entry for the specified tab.
  void UpdateTabNavigation(const SessionID& window_id,
                           const SessionID& tab_id,
                           int index,
                           const NavigationEntry& entry);

  // Notification that a tab has restored its entries or a closed tab is being
  // reused.
  void TabRestored(NavigationController* controller);

  // Sets the index of the selected entry in the navigation controller for the
  // specified tab.
  void SetSelectedNavigationIndex(const SessionID& window_id,
                                  const SessionID& tab_id,
                                  int index);

  // Sets the index of the selected tab in the specified window.
  void SetSelectedTabInWindow(const SessionID& window_id, int index);

  // Callback from GetSavedSession of GetLastSession.
  //
  // The contents of the supplied vector are deleted after the callback is
  // notified. To take ownership of the vector clear it before returning.
  //
  // The time gives the time the session was closed.
  typedef Callback2<Handle, std::vector<SessionWindow*>*>::Type
      SavedSessionCallback;

  // Fetches the contents of the save session, notifying the callback when
  // done. If the callback is supplied an empty vector of SessionWindows
  // it means the session could not be restored.
  Handle GetSavedSession(CancelableRequestConsumerBase* consumer,
                         SavedSessionCallback* callback);

  // Fetches the contents of the last session, notifying the callback when
  // done. If the callback is supplied an empty vector of SessionWindows
  // it means the session could not be restored.
  Handle GetLastSession(CancelableRequestConsumerBase* consumer,
                        SavedSessionCallback* callback);

  // Creates a save session from the current state of the browser.
  void CreateSavedSession();

  // Deletes the saved session if saved session is true, or the last session
  // if saved_session is false.
  void DeleteSession(bool saved_session);

  // Creates a saved session from the contents of the last session.
  void CopyLastSessionToSavedSession();

  // The callback from Get*Session is internally routed to SessionService
  // first. This is done so that the SessionWindows can be recreated from
  // the SessionCommands. The following types are used for this.
  class InternalSavedSessionRequest;

  typedef Callback2<Handle, scoped_refptr<InternalSavedSessionRequest> >::Type
      InternalSavedSessionCallback;

  // Request class used from Get*Session.
  class InternalSavedSessionRequest :
      public CancelableRequest<InternalSavedSessionCallback> {
   public:
    InternalSavedSessionRequest(CallbackType* callback,
                                SavedSessionCallback* real_callback,
                                bool is_saved_session)
        : CancelableRequest(callback),
          real_callback(real_callback),
          is_saved_session(is_saved_session) {
    }
    virtual ~InternalSavedSessionRequest();

    // The callback supplied to Get*Session.
    scoped_ptr<SavedSessionCallback> real_callback;

    // Whether the request is for a saved session, or the last session.
    bool is_saved_session;

    // The commands. The backend fills this in for us.
    std::vector<SessionCommand*> commands;

   private:
    DISALLOW_EVIL_CONSTRUCTORS(InternalSavedSessionRequest);
  };

 private:
  typedef std::map<SessionID::id_type,std::pair<int,int> > IdToRange;
  typedef std::map<SessionID::id_type,SessionTab*> IdToSessionTab;
  typedef std::map<SessionID::id_type,SessionWindow*> IdToSessionWindow;

  // Various initialization; called from the constructor.
  void Init(const std::wstring& path);

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Get*Session call into this to schedule the request. The request
  // does NOT directly invoke the callback, rather the callback invokes
  // OnGotSessionCommands from which we map the SessionCommands to browser
  // state, then notify the callback.
  Handle GetSessionImpl(CancelableRequestConsumerBase* consumer,
                        SavedSessionCallback* callback,
                        bool is_saved_session);

  // Methods to create the various commands. It is up to the caller to delete
  // the returned the SessionCommand* object.
  SessionCommand* CreateSetSelectedTabInWindow(const SessionID& window_id,
                                               int index);

  SessionCommand* CreateSetTabWindowCommand(const SessionID& window_id,
                                            const SessionID& tab_id);

  SessionCommand* CreateSetWindowBoundsCommand(const SessionID& window_id,
                                               const gfx::Rect& bounds,
                                               bool is_maximized);

  SessionCommand* CreateSetTabIndexInWindowCommand(const SessionID& tab_id,
                                                   int new_index);

  SessionCommand* CreateTabClosedCommand(SessionID::id_type tab_id);

  SessionCommand* CreateWindowClosedCommand(SessionID::id_type tab_id);

  SessionCommand* CreateUpdateTabNavigationCommand(
      const SessionID& tab_id,
      int index,
      const NavigationEntry& entry);

  SessionCommand* CreateSetSelectedNavigationIndexCommand(
      const SessionID& tab_id,
      int index);

  SessionCommand* CreateSetWindowTypeCommand(const SessionID& window_id,
                                             BrowserType::Type type);

  // Callback form the backend for getting the commands from the previous
  // or save file. Converts the commands in SessionWindows and notifies
  // the real callback.
  void OnGotSessionCommands(
      Handle handle,
      scoped_refptr<InternalSavedSessionRequest> request);

  // Converts the commands into SessionWindows. On return any valid
  // windows are added to valid_windows. It is up to the caller to delete
  // the windows added to valid_windows.
  //
  // If ignore_recent_closes is true, any window/tab closes within in a certain
  // time frame are ignored.
  void RestoreSessionFromCommands(const std::vector<SessionCommand*>& commands,
                                  std::vector<SessionWindow*>* valid_windows);

  // Iterates through the vector updating the selected_tab_index of each
  // SessionWindow based on the actual tabs that were restored.
  void UpdateSelectedTabIndex(std::vector<SessionWindow*>* windows);

  // Returns the window in windows with the specified id. If a window does
  // not exist, one is created.
  SessionWindow* GetWindow(SessionID::id_type window_id,
                           IdToSessionWindow* windows);

  // Returns the tab with the specified id in tabs. If a tab does not exist,
  // it is created.
  SessionTab* GetTab(SessionID::id_type tab_id,
                     IdToSessionTab* tabs);

  // Returns an iterator into navigations pointing to the navigation whose
  // index matches |index|. If no navigation index matches |index|, the first
  // navigation with an index > |index| is returned.
  //
  // This assumes the navigations are ordered by index in ascending order.
  std::vector<TabNavigation>::iterator FindClosestNavigationWithIndex(
      std::vector<TabNavigation>* navigations,
      int index);

  // Does the following:
  // . Deletes and removes any windows with no tabs or windows with types other
  //   than tabbed_browser or browser. NOTE: constrained windows that have
  //   been dragged out are of type browser. As such, this preserves any dragged
  //   out constrained windows (aka popups that have been dragged out).
  // . Sorts the tabs in windows with valid tabs based on the tabs
  //   visual order, and adds the valid windows to windows.
  void SortTabsBasedOnVisualOrderAndPrune(
      std::map<int,SessionWindow*>* windows,
      std::vector<SessionWindow*>* valid_windows);

  // Adds tabs to their parent window based on the tab's window_id. This
  // ignores tabs with no navigations.
  void AddTabsToWindows(std::map<int,SessionTab*>* tabs,
                        std::map<int,SessionWindow*>* windows);

  // Creates tabs and windows from the specified commands. The created tabs
  // and windows are added to |tabs| and |windows| respectively. It is up to
  // the caller to delete the tabs and windows added to |tabs| and |windows|.
  //
  // This does NOT add any created SessionTabs to SessionWindow.tabs, that is
  // done by AddTabsToWindows.
  bool CreateTabsAndWindows(const std::vector<SessionCommand*>& data,
                            std::map<int,SessionTab*>* tabs,
                            std::map<int,SessionWindow*>* windows);

  // Adds commands to commands that will recreate the state of the specified
  // NavigationController. This adds at most kMaxNavigationCountToPersist
  // navigations (in each direction from the current navigation index).
  // A pair is added to tab_to_available_range indicating the range of
  // indices that were written.
  void BuildCommandsForTab(
      const SessionID& window_id,
      NavigationController* controller,
      int index_in_window,
      std::vector<SessionCommand*>* commands,
      IdToRange* tab_to_available_range);

  // Adds commands to create the specified browser, and invokes
  // BuildCommandsForTab for each of the tabs in the browser. This ignores
  // any tabs not in the profile we were created with.
  void BuildCommandsForBrowser(
      Browser* browser,
      std::vector<SessionCommand*>* commands,
      IdToRange* tab_to_available_range,
      std::set<SessionID::id_type>* windows_to_track);

  // Iterates over all the known browsers invoking BuildCommandsForBrowser.
  // This only adds browsers that should be tracked
  // (should_track_changes_for_browser_type returns true). All browsers that
  // are tracked are added to windows_to_track (as long as it is non-null).
  void BuildCommandsFromBrowsers(
      std::vector<SessionCommand*>* commands,
      IdToRange* tab_to_available_range,
      std::set<SessionID::id_type>* windows_to_track);

  // Schedules a reset. A reset means the contents of the file are recreated
  // from the state of the browser.
  void ScheduleReset();

  // Searches for a pending command that can be replaced with command.
  // If one is found, pending command is removed, command is added to
  // the pending commands and true is returned.
  bool ReplacePendingCommand(SessionCommand* command);

  // Schedules the specified command. This method takes ownership of the
  // command.
  void ScheduleCommand(SessionCommand* command);

  // Converts all pending tab/window closes to commands and schedules them.
  void CommitPendingCloses();

  // Saves pending commands to the backend.
  void Save();

  // Starts the save timer (if it isn't running already).
  void StartSaveTimer();

  // Returns true if there is only one window open with a single tab that shares
  // our profile.
  bool IsOnlyOneTabLeft();

  // Returns true if there are no open tabbed browser windows with our profile,
  // or the only tabbed browser open has a session id of window_id.
  bool HasOpenTabbedBrowsers(const SessionID& window_id);

  // Returns true if changes to tabs in the specified window should be tracked.
  bool ShouldTrackChangesToWindow(const SessionID& window_id);

  // Should we track the specified entry?
  bool SessionService::ShouldTrackEntry(const NavigationEntry& entry);

  // Returns true if we track changes to the specified browser type.
  static bool should_track_changes_for_browser_type(BrowserType::Type type) {
    return type == BrowserType::TABBED_BROWSER;
  }

  // The profile used to determine where to save, as well as what tabs
  // to persist.
  Profile* profile_;

  // The number of commands sent to the backend before doing a reset.
  int commands_since_reset_;

  // Maps from session tab id to the range of navigation entries that has
  // been written to disk.
  //
  // This is only used if not all the navigation entries have been
  // written.
  IdToRange tab_to_available_range_;

  // Commands we need to send over to the backend.
  std::vector<SessionCommand*>  pending_commands_;

  // Whether the backend file should be recreated the next time we send
  // over the commands.
  bool pending_reset_;

  // Used to invoke Save.
  ScopedRunnableMethodFactory<SessionService> save_factory_;

  // When the user closes the last window, where the last window is the
  // last tabbed browser and no more tabbed browsers are open with the same
  // profile, the window ID is added here. These IDs are only committed (which
  // marks them as closed) if the user creates a new tabbed browser.
  typedef std::set<SessionID::id_type> PendingWindowCloseIDs;
  PendingWindowCloseIDs pending_window_close_ids_;

  // Set of tabs that have been closed by way of the last window or last tab
  // closing, but not yet committed.
  typedef std::set<SessionID::id_type> PendingTabCloseIDs;
  PendingTabCloseIDs pending_tab_close_ids_;

  // When a window other than the last window (see description of
  // pending_window_close_ids) is closed, the id is added to this set.
  typedef std::set<SessionID::id_type> WindowClosingIDs;
  WindowClosingIDs window_closing_ids_;

  // Set of windows we're tracking changes to. This is only browsers that
  // return true from should_track_changes_for_browser_type.
  typedef std::set<SessionID::id_type> WindowsTracking;
  WindowsTracking windows_tracking_;

  // The backend.
  scoped_refptr<SessionBackend> backend_;

  // Thread backend tasks are run on. This comes from the profile, and is
  // null during testing.
  base::Thread* backend_thread_;

  // Are there any open open tabbed browsers?
  bool has_open_tabbed_browsers_;

  // If true and a new tabbed browser is created and there are no opened tabbed
  // browser (has_open_tabbed_browsers_ is false), then the current session
  // is made the previous session. See description above class for details on
  // current/previou session.
  bool move_on_new_browser_;
};

#endif  // CHROME_BROWSER_SESSION_SERVICE_H__

