// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_RESTORE_SERVICE_H__
#define CHROME_BROWSER_TAB_RESTORE_SERVICE_H__

#include <list>

#include "base/observer_list.h"
#include "base/time.h"
#include "chrome/browser/session_service.h"

class NavigationController;
class Profile;

// TabRestoreService is responsible for maintaining the most recently closed
// tabs. When the user closes a tab TabRestoreService::CreateHistoricalTab is
// invoked and a HistoricalTab is created to represent the tab.
//
// TabRestoreService can recreate tabs from the previous session as well.
// LoadPreviousSessionTabs loads (asynchronously) the tabs from the previous
// session. When done, the observer is notified.
//
// To restore a tab from the TabRestoreService invoke AddRestoredTab on the
// Browser you want to restore the tab to, followed by RemoveHistoricalTabById
// to remove the tab from the restore service.
//
// To listen for changes to the set of tabs managed by the TabRestoreService
// add an observer.

class TabRestoreService {
 public:
  // Observer is notified when the set of tabs managed by TabRestoreService
  // changes in some way.
  class Observer {
   public:
    // Sent when the internal tab state changed
    virtual void TabRestoreServiceChanged(TabRestoreService* service) = 0;

    // Sent to all remaining Observers when TabRestoreService's
    // destructor is run.
    virtual void TabRestoreServiceDestroyed(TabRestoreService* service) = 0;
  };

  // Represents a previously open tab.
  struct HistoricalTab {
    HistoricalTab();

    // Time the tab was closed.
    Time close_time;

    // If true, this is a historical session and not a closed tab.
    bool from_last_session;

    // The navigations.
    // WARNING: navigations may be empty.
    std::vector<TabNavigation> navigations;

    // Index of the selected navigation in navigations.
    int current_navigation_index;

    // Unique id for the closed tab. This is guaranteed to be unique for
    // a session.
    const int id;
  };

  typedef std::list<HistoricalTab> Tabs;

  // Creates a new TabRestoreService. This does not load tabs from the last
  // session, you must explicitly invoke LoadPreviousSessionTabs to do that.
  explicit TabRestoreService(Profile* profile);
  ~TabRestoreService();

  // Adds/removes an observer. TabRestoreService does not take ownership of
  // the observer.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // If the previous session has not been loaded, it is loaded and the tabs
  // from it are placed at the end of the queue.
  void LoadPreviousSessionTabs();

  // Returns true if loading the previous sessions tabs.
  bool IsLoadingPreviousSessionTabs();

  // Returns true if LoadPreviousSessionTabs will attempt to load any tabs.
  bool WillLoadPreviousSessionTabs();

  // Creates a HistoricalTab to represent the tab and notifies observers the
  // list of tabs has changed.
  void CreateHistoricalTab(NavigationController* tab);

  // Removes the HistoricalTab with the specified id and notifies observers.
  // Does nothing if id does not identify a valid historical tab id.
  void RemoveHistoricalTabById(int id);

  // Removes all HistoricalTabs from the list and notifies observers the list
  // of tabs has changed.
  void ClearHistoricalTabs();

  // Returns the tabs, ordered with most recently closed tabs at the front.
  const Tabs& tabs() const { return tabs_; }

 private:
  // Callback from loading the last session. As necessary adds the tabs to
  // tabs_.
  void OnGotLastSession(SessionService::Handle handle,
                        std::vector<SessionWindow*>* windows);

  // Invoked from OnGotLastSession to add the necessary tabs from windows
  // to tabs_.
  void AddHistoricalTabs(std::vector<SessionWindow*>* windows);

  // Creates a HistoricalTab from the tab.
  void AppendHistoricalTabFromSessionTab(SessionTab* tab);

  // Populates tabs->navigations from the NavigationController.
  void PopulateTabFromController(NavigationController* controller,
                                 HistoricalTab* tab);

  // Populates tab->navigations from a previous sessions navigations.
  void PopulateTabFromSessionTab(SessionTab* session_tab,
                                 HistoricalTab* tab);

  // Notifies observers the tabs have changed.
  void NotifyTabsChanged();

  Profile* profile_;

  // Whether we've loaded the last session.
  bool loaded_last_session_;

  // Set of tabs. We own the NavigationControllers in this list.
  Tabs tabs_;

  // Used in requesting the last session.
  CancelableRequestConsumer cancelable_consumer_;

  ObserverList<Observer> observer_list_;

  DISALLOW_EVIL_CONSTRUCTORS(TabRestoreService);
};

#endif  // CHROME_BROWSER_TAB_RESTORE_SERVICE_H__

