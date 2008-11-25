// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_RESTORE_SERVICE_H_
#define CHROME_BROWSER_TAB_RESTORE_SERVICE_H_

#include <list>

#include "base/observer_list.h"
#include "base/time.h"
#include "chrome/browser/session_service.h"

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
class TabRestoreService {
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
    int id;

    // The type of the entry.
    Type type;
  };

  // Represents a previously open tab.
  struct Tab : public Entry {
    Tab() : Entry(TAB), current_navigation_index(-1) {}

    // The navigations.
    // WARNING: navigations may be empty.
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
  ~TabRestoreService();

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
  void RestoreEntryById(Browser* browser, int id, bool replace_existing_tab);

 private:
  // Populates tabs->navigations from the NavigationController.
  void PopulateTabFromController(NavigationController* controller,
                                 Tab* tab);

  // Notifies observers the tabs have changed.
  void NotifyTabsChanged();

  // Prunes entries_ to contain only kMaxEntries and invokes NotifyTabsChanged.
  void PruneAndNotify();

  // Returns an iterator into entries_ whose id matches |id|.
  Entries::iterator GetEntryIteratorById(int id);

  Profile* profile_;

  // Whether we've loaded the last session.
  bool loaded_last_session_;

  // Set of entries.
  Entries entries_;

  // Are we restoring a tab? If this is true we ignore requests to create a
  // historical tab.
  bool restoring_;

  ObserverList<Observer> observer_list_;

  // Set of tabs that we've received a BrowserClosing method for but no
  // corresponding BrowserClosed. We cache the set of browsers closing to
  // avoid creating historical tabs for them.
  std::set<Browser*> closing_browsers_;

  DISALLOW_COPY_AND_ASSIGN(TabRestoreService);
};

#endif  // CHROME_BROWSER_TAB_RESTORE_SERVICE_H_
