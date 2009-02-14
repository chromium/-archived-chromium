// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_TAB_RESTORE_SERVICE_H_
#define CHROME_BROWSER_SESSIONS_TAB_RESTORE_SERVICE_H_

#include <list>
#include <vector>

#include "base/observer_list.h"
#include "base/time.h"
#include "chrome/browser/sessions/base_session_service.h"
#include "chrome/browser/sessions/session_id.h"
#include "chrome/browser/sessions/session_types.h"

class Browser;
class NavigationController;
class Profile;

// TabRestoreService is responsible for maintaining the most recently closed
// tabs and windows. When a tab is closed
// TabRestoreService::CreateHistoricalTab is invoked and a Tab is created to
// represent the tab. Similarly, when a browser is closed, BrowserClosing is
// invoked and a Window is created to represent the window.
//
// To restore a tab/window from the TabRestoreService invoke RestoreEntryById
// or RestoreMostRecentEntry.
//
// To listen for changes to the set of entries managed by the TabRestoreService
// add an observer.
class TabRestoreService : public BaseSessionService {
 public:
  // Observer is notified when the set of entries managed by TabRestoreService
  // changes in some way.
  class Observer {
   public:
    // Sent when the set of entries changes in some way.
    virtual void TabRestoreServiceChanged(TabRestoreService* service) = 0;

    // Sent to all remaining Observers when TabRestoreService's
    // destructor is run.
    virtual void TabRestoreServiceDestroyed(TabRestoreService* service) = 0;
  };

  // The type of entry.
  enum Type {
    TAB,
    WINDOW
  };

  struct Entry {
    Entry();
    explicit Entry(Type type);
    virtual ~Entry() {}

    // Unique id for this entry. The id is guaranteed to be unique for a
    // session.
    SessionID::id_type id;

    // The type of the entry.
    Type type;
  };

  // Represents a previously open tab.
  struct Tab : public Entry {
    Tab() : Entry(TAB), current_navigation_index(-1) {}

    // The navigations.
    std::vector<TabNavigation> navigations;

    // Index of the selected navigation in navigations.
    int current_navigation_index;
  };

  // Represents a previously open window.
  struct Window : public Entry {
    Window() : Entry(WINDOW), selected_tab_index(-1) {}

    // The tabs that comprised the window, in order.
    std::vector<Tab> tabs;

    // Index of the selected tab.
    int selected_tab_index;
  };

  typedef std::list<Entry*> Entries;

  // Creates a new TabRestoreService.
  explicit TabRestoreService(Profile* profile);
  virtual ~TabRestoreService();

  // Adds/removes an observer. TabRestoreService does not take ownership of
  // the observer.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Creates a Tab to represent |tab| and notifies observers the list of
  // entries has changed.
  void CreateHistoricalTab(NavigationController* tab);

  // Invoked when a browser is closing. If |browser| is a tabbed browser with
  // at least one tab, a Window is created, added to entries and observers are
  // notified.
  void BrowserClosing(Browser* browser);

  // Invoked when the browser is done closing.
  void BrowserClosed(Browser* browser);

  // Removes all entries from the list and notifies observers the list
  // of tabs has changed.
  void ClearEntries();

  // Returns the entries, ordered with most recently closed entries at the
  // front.
  const Entries& entries() const { return entries_; }

  // Restores the most recently closed entry. Does nothing if there are no
  // entries to restore. If the most recently restored entry is a tab, it is
  // added to |browser|.
  void RestoreMostRecentEntry(Browser* browser);

  // Restores an entry by id. If there is no entry with an id matching |id|,
  // this does nothing. If |replace_existing_tab| is true and id identifies a
  // tab, the newly created tab replaces the selected tab in |browser|.
  void RestoreEntryById(Browser* browser,
                        SessionID::id_type id,
                        bool replace_existing_tab);

  // Loads the tabs and previous session. This does nothing if the tabs
  // from the previous session have already been loaded.
  void LoadTabsFromLastSession();

  // Max number of entries we'll keep around.
  static const size_t kMaxEntries;

 protected:
  virtual void Save();

 private:
  // Used to indicate what has loaded.
  enum LoadState {
    // Indicates we haven't loaded anything.
    NOT_LOADED           = 1 << 0,

    // Indicates we've asked for the last sessions and tabs but haven't gotten
    // the result back yet.
    LOADING              = 1 << 2,

    // Indicates we finished loading the last tabs (but not necessarily the
    // last session).
    LOADED_LAST_TABS     = 1 << 3,

    // Indicates we finished loading the last session (but not necessarily the
    // last tabs).
    LOADED_LAST_SESSION  = 1 << 4
  };

  // Populates tabs->navigations from the NavigationController.
  void PopulateTabFromController(NavigationController* controller,
                                 Tab* tab);

  // Notifies observers the tabs have changed.
  void NotifyTabsChanged();

  // Adds |entry| to the list of entries. If |prune| is true |PruneAndNotify|
  // is invoked. If |to_front| is true the entry is added to the front,
  // otherwise the back. Normal closes go to the front, but tab/window closes
  // from the previous session are added to the back.
  void AddEntry(Entry* entry, bool prune, bool to_front);

  // Prunes entries_ to contain only kMaxEntries and invokes NotifyTabsChanged.
  void PruneAndNotify();

  // Returns an iterator into entries_ whose id matches |id|.
  Entries::iterator GetEntryIteratorById(SessionID::id_type id);

  // Schedules the commands for a window close.
  void ScheduleCommandsForWindow(const Window& window);

  // Schedules the commands for a tab close. |selected_index| gives the
  // index of the selected navigation.
  void ScheduleCommandsForTab(const Tab& tab, int selected_index);

  // Creates a window close command.
  SessionCommand* CreateWindowCommand(SessionID::id_type id,
                                      int selected_tab_index,
                                      int num_tabs);

  // Creates a tab close command.
  SessionCommand* CreateSelectedNavigationInTabCommand(
      SessionID::id_type tab_id,
      int32 index);

  // Creates a restore command.
  SessionCommand* CreateRestoredEntryCommand(SessionID::id_type entry_id);

  // Returns the index to persist as the selected index. This is the same
  // as |tab.current_navigation_index| unless the entry at
  // |tab.current_navigation_index| shouldn't be persisted. Returns -1 if
  // no valid navigation to persist.
  int GetSelectedNavigationIndexToPersist(const Tab& tab);

  // Invoked when we've loaded the session commands that identify the
  // previously closed tabs. This creates entries, adds them to
  // staging_entries_, and invokes LoadState.
  void OnGotLastSessionCommands(
      Handle handle,
      scoped_refptr<InternalGetCommandsRequest> request);

  // Populates |loaded_entries| with Entries from |request|.
  void CreateEntriesFromCommands(
      scoped_refptr<InternalGetCommandsRequest> request,
      std::vector<Entry*>* loaded_entries);

  // Returns true if |tab| has more than one navigation. If |tab| has more
  // than one navigation |tab->current_navigation_index| is constrained based
  // on the number of navigations.
  bool ValidateTab(Tab* tab);

  // Validates all entries in |entries|, deleting any with no navigations.
  // This also deletes any entries beyond the max number of entries we can
  // hold.
  void ValidateAndDeleteEmptyEntries(std::vector<Entry*>* entries);

  // Callback from SessionService when we've received the windows from the
  // previous session. This creates and add entries to |staging_entries_|
  // and invokes LoadStateChanged.
  void OnGotPreviousSession(Handle handle,
                            std::vector<SessionWindow*>* windows);

  // Creates and add entries to |entries| for each of the windows in |windows|.
  void CreateEntriesFromWindows(
      std::vector<SessionWindow*>* windows,
      std::vector<Entry*>* entries);

  // Converts a SessionWindow into a Window, returning true on success.
  bool ConvertSessionWindowToWindow(
      SessionWindow* session_window,
      Window* window);

  // Invoked when previous tabs or session is loaded. If both have finished
  // loading the entries in staging_entries_ are added to entries_ and
  // observers are notified.
  void LoadStateChanged();

  // Set of entries.
  Entries entries_;

  // Whether we've loaded the last session.
  int load_state_;

  // Are we restoring a tab? If this is true we ignore requests to create a
  // historical tab.
  bool restoring_;

  // Have the max number of entries ever been created?
  bool reached_max_;

  // The number of entries to write.
  int entries_to_write_;

  // Number of entries we've written.
  int entries_written_;

  ObserverList<Observer> observer_list_;

  // Set of tabs that we've received a BrowserClosing method for but no
  // corresponding BrowserClosed. We cache the set of browsers closing to
  // avoid creating historical tabs for them.
  std::set<Browser*> closing_browsers_;

  // Used when loading previous tabs/session.
  CancelableRequestConsumer load_consumer_;

  // Results from previously closed tabs/sessions is first added here. When
  // the results from both us and the session restore service have finished
  // loading LoadStateChanged is invoked, which adds these entries to
  // entries_.
  std::vector<Entry*> staging_entries_;

  DISALLOW_COPY_AND_ASSIGN(TabRestoreService);
};

#endif  // CHROME_BROWSER_SESSIONS_TAB_RESTORE_SERVICE_H_
