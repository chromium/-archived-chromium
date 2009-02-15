// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/session_restore.h"

#include <vector>

#include "base/scoped_ptr.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_types.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_service.h"

namespace {

// TabLoader ------------------------------------------------------------------

// TabLoader is responsible for ensuring after session restore we have
// at least SessionRestore::num_tabs_to_load_ loading. As tabs finish loading
// new tabs are loaded. When all tabs are loading TabLoader deletes itself.
//
// This is not part of SessionRestoreImpl so that synchronous destruction
// of SessionRestoreImpl doesn't have timing problems.

class TabLoader : public NotificationObserver {
 public:
  typedef std::list<NavigationController*> TabsToLoad;

  TabLoader();
  ~TabLoader();

  // Adds a tab to load.
  void AddTab(NavigationController* controller);

  // Loads the next batch of tabs until SessionRestore::num_tabs_to_load_ tabs
  // are loading, or all tabs are loading. If there are no more tabs to load,
  // this deletes the TabLoader.
  //
  // This must be invoked once to start loading.
  void LoadTabs();

 private:
  // NotificationObserver method. Removes the specified tab and loads the next
  // tab.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Removes the listeners from the specified tab and removes the tab from
  // the set of tabs to load and list of tabs we're waiting to get a load
  // from.
  void RemoveTab(NavigationController* tab);

  // Adds the necessary listeners for the specified tab.
  void AddListeners(NavigationController* controller);

  // Removes the necessary listeners for the specified tab.
  void RemoveListeners(NavigationController* controller);

  // Has Load been invoked?
  bool loading_;

  // The set of tabs we've initiated loading on. This does NOT include the
  // selected tabs.
  typedef std::set<NavigationController*> TabsLoading;
  TabsLoading tabs_loading_;

  // The tabs we need to load.
  TabsToLoad tabs_to_load_;
};

TabLoader::TabLoader()
    : loading_(false) {
}

TabLoader::~TabLoader() {
  DCHECK(tabs_to_load_.empty() && tabs_loading_.empty());
}

void TabLoader::AddTab(NavigationController* controller) {
  if (controller) {
    DCHECK(find(tabs_to_load_.begin(), tabs_to_load_.end(), controller) ==
           tabs_to_load_.end());
    tabs_to_load_.push_back(controller);
    AddListeners(controller);
  } else {
    // Should never get a NULL tab.
    NOTREACHED();
  }
}

void TabLoader::LoadTabs() {
  loading_ = true;
  while (!tabs_to_load_.empty() &&
         (SessionRestore::num_tabs_to_load_ == 0 ||
          tabs_loading_.size() < SessionRestore::num_tabs_to_load_)) {
    NavigationController* tab = tabs_to_load_.front();
    tabs_loading_.insert(tab);
    tabs_to_load_.pop_front();
    tab->LoadIfNecessary();
    if (tab && tab->active_contents()) {
      int tab_index;
      Browser* browser = Browser::GetBrowserForController(tab, &tab_index);
      if (browser && browser->selected_index() != tab_index) {
        // By default tabs are marked as visible. As only the selected tab is
        // visible we need to explicitly tell non-selected tabs they are hidden.
        // Without this call non-selected tabs are not marked as backgrounded.
        //
        // NOTE: We need to do this here rather than when the tab is added to
        // the Browser as at that time not everything has been created, so that
        // the call would do nothing.
        tab->active_contents()->WasHidden();
      }
    }
  }

  if (tabs_to_load_.empty()) {
    for (TabsLoading::iterator i = tabs_loading_.begin();
         i != tabs_loading_.end(); ++i) {
      RemoveListeners(*i);
    }
    tabs_loading_.clear();
    delete this;
  }
}

void TabLoader::Observe(NotificationType type,
                        const NotificationSource& source,
                        const NotificationDetails& details) {
  DCHECK(type == NotificationType::TAB_CLOSED ||
         type == NotificationType::LOAD_STOP);
  NavigationController* tab = Source<NavigationController>(source).ptr();
  RemoveTab(tab);
  if (loading_) {
    LoadTabs();
    // WARNING: if there are no more tabs to load, we have been deleted.
  }
}

void TabLoader::RemoveTab(NavigationController* tab) {
  RemoveListeners(tab);

  TabsLoading::iterator i = tabs_loading_.find(tab);
  if (i != tabs_loading_.end())
    tabs_loading_.erase(i);

  TabsToLoad::iterator j =
      find(tabs_to_load_.begin(), tabs_to_load_.end(), tab);
  if (j != tabs_to_load_.end())
    tabs_to_load_.erase(j);
}

void TabLoader::AddListeners(NavigationController* controller) {
  NotificationService::current()->AddObserver(
      this, NotificationType::TAB_CLOSED,
      Source<NavigationController>(controller));
  NotificationService::current()->AddObserver(
      this, NotificationType::LOAD_STOP,
      Source<NavigationController>(controller));
}

void TabLoader::RemoveListeners(NavigationController* controller) {
  NotificationService::current()->RemoveObserver(
      this, NotificationType::TAB_CLOSED,
      Source<NavigationController>(controller));
  NotificationService::current()->RemoveObserver(
      this, NotificationType::LOAD_STOP,
      Source<NavigationController>(controller));
}

// SessionRestoreImpl ---------------------------------------------------------

// SessionRestoreImpl is responsible for fetching the set of tabs to create
// from SessionService. SessionRestoreImpl deletes itself when done.

class SessionRestoreImpl : public NotificationObserver {
 public:
  SessionRestoreImpl(Profile* profile,
                     Browser* browser,
                     bool synchronous,
                     bool clobber_existing_window,
                     bool always_create_tabbed_browser,
                     const std::vector<GURL>& urls_to_open)
      : profile_(profile),
        browser_(browser),
        synchronous_(synchronous),
        clobber_existing_window_(clobber_existing_window),
        always_create_tabbed_browser_(always_create_tabbed_browser),
        urls_to_open_(urls_to_open) {
  }

  void Restore() {
    SessionService* session_service = profile_->GetSessionService();
    DCHECK(session_service);
    SessionService::LastSessionCallback* callback =
        NewCallback(this, &SessionRestoreImpl::OnGotSession);
    session_service->GetLastSession(&request_consumer_, callback);

    if (synchronous_) {
      MessageLoop::current()->Run();
      delete this;
      return;
    }

    if (browser_) {
      registrar_.Add(this, NotificationType::BROWSER_CLOSED,
                     Source<Browser>(browser_));
    }
  }

  ~SessionRestoreImpl() {
  }

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    if (type != NotificationType::BROWSER_CLOSED) {
      NOTREACHED();
      return;
    }
    delete this;
  }

 private:
  // Invoked when done with creating all the tabs/browsers.
  //
  // |created_tabbed_browser| indicates whether a tabbed browser was created,
  // or we used an existing tabbed browser.
  //
  // If successful, this begins loading tabs and deletes itself when all tabs
  // have been loaded.
  void FinishedTabCreation(bool succeeded, bool created_tabbed_browser) {
    if (!created_tabbed_browser && always_create_tabbed_browser_) {
      Browser* browser = Browser::Create(profile_);
      if (urls_to_open_.empty()) {
        // No tab browsers were created and no URLs were supplied on the command
        // line. Add an empty URL, which is treated as opening the users home
        // page.
        urls_to_open_.push_back(GURL());
      }
      AppendURLsToBrowser(browser, urls_to_open_);
      browser->window()->Show();
    }

    if (synchronous_)
      MessageLoop::current()->Quit();

    if (succeeded) {
      DCHECK(tab_loader_.get());
      // TabLoader delets itself when done loading.
      tab_loader_.release()->LoadTabs();
    }

    if (!synchronous_) {
      // If we're not synchronous we need to delete ourself.
      // NOTE: we must use DeleteLater here as most likely we're in a callback
      // from the history service which doesn't deal well with deleting the
      // object it is notifying.
      MessageLoop::current()->DeleteSoon(FROM_HERE, this);
    }
  }

  void OnGotSession(SessionService::Handle handle,
                    std::vector<SessionWindow*>* windows) {
    if (windows->empty()) {
      // Restore was unsuccessful.
      FinishedTabCreation(false, false);
      return;
    }

    tab_loader_.reset(new TabLoader());

    Browser* current_browser =
        browser_ ? browser_ : BrowserList::GetLastActive();
    // After the for loop this contains the last TABBED_BROWSER. Is null if no
    // tabbed browsers exist.
    Browser* last_browser = NULL;
    bool has_tabbed_browser = false;
    for (std::vector<SessionWindow*>::iterator i = windows->begin();
         i != windows->end(); ++i) {
      Browser* browser = NULL;
      if (!has_tabbed_browser && (*i)->type == Browser::TYPE_NORMAL)
        has_tabbed_browser = true;
      if (i == windows->begin() && (*i)->type == Browser::TYPE_NORMAL &&
          !clobber_existing_window_) {
        // If there is an open tabbed browser window, use it. Otherwise fall
        // through and create a new one.
        browser = current_browser;
        if (browser && (browser->type() != Browser::TYPE_NORMAL ||
                        browser->profile()->IsOffTheRecord())) {
          browser = NULL;
        }
      }
      if (!browser) {
        browser = new Browser((*i)->type, profile_);
        browser->set_override_bounds((*i)->bounds);
        browser->set_override_maximized((*i)->is_maximized);
        browser->CreateBrowserWindow();
      }
      if ((*i)->type == Browser::TYPE_NORMAL)
        last_browser = browser;
      const int initial_tab_count = browser->tab_count();
      RestoreTabsToBrowser(*(*i), browser);
      ShowBrowser(browser, initial_tab_count, (*i)->selected_tab_index);
      NotifySessionServiceOfRestoredTabs(browser, initial_tab_count);
    }

    // If we're restoring a session as the result of a crash and the session
    // included at least one tabbed browser, then close the browser window
    // that was opened when the user clicked to restore the session.
    if (clobber_existing_window_ && current_browser && has_tabbed_browser &&
        current_browser->type() == Browser::TYPE_NORMAL) {
      current_browser->CloseAllTabs();
    }
    if (last_browser && !urls_to_open_.empty())
      AppendURLsToBrowser(last_browser, urls_to_open_);
    // If last_browser is NULL and urls_to_open_ is non-empty,
    // FinishedTabCreation will create a new TabbedBrowser and add the urls to
    // it.
    FinishedTabCreation(true, has_tabbed_browser);
  }

  void RestoreTabsToBrowser(const SessionWindow& window, Browser* browser) {
    DCHECK(!window.tabs.empty());
    for (std::vector<SessionTab*>::const_iterator i = window.tabs.begin();
         i != window.tabs.end(); ++i) {
      const SessionTab& tab = *(*i);
      DCHECK(!tab.navigations.empty());
      int selected_index = tab.current_navigation_index;
      selected_index = std::max(
          0,
          std::min(selected_index,
                   static_cast<int>(tab.navigations.size() - 1)));
      tab_loader_->AddTab(
          browser->AddRestoredTab(tab.navigations,
                                  static_cast<int>(i - window.tabs.begin()),
                                  selected_index,
                                  false));
    }
  }

  void ShowBrowser(Browser* browser,
                   int initial_tab_count,
                   int selected_session_index) {
    if (browser_ == browser) {
      browser->SelectTabContentsAt(browser->tab_count() - 1, true);
      return;
    }

    DCHECK(browser);
    DCHECK(browser->tab_count());
    browser->SelectTabContentsAt(
        std::min(initial_tab_count + std::max(0, selected_session_index),
                 browser->tab_count() - 1), true);
    browser->window()->Show();
  }

  void AppendURLsToBrowser(Browser* browser, const std::vector<GURL>& urls) {
    for (size_t i = 0; i < urls.size(); ++i) {
      browser->AddTabWithURL(urls[i], GURL(), PageTransition::START_PAGE,
                            (i == 0), NULL);
    }
  }

  // Invokes TabRestored on the SessionService for all tabs in browser after
  // initial_count.
  void NotifySessionServiceOfRestoredTabs(Browser* browser, int initial_count) {
    SessionService* session_service = profile_->GetSessionService();
    for (int i = initial_count; i < browser->tab_count(); ++i)
      session_service->TabRestored(browser->GetTabContentsAt(i)->controller());
  }

  // The profile to create the sessions for.
  Profile* profile_;

  // The first browser to restore to, may be null.
  Browser* browser_;

  // Whether or not restore is synchronous.
  const bool synchronous_;

  // See description in RestoreSession (in .h).
  const bool clobber_existing_window_;

  // If true and there is an error or there are no windows to restore, we
  // create a tabbed browser anyway. This is used on startup to make sure at
  // at least one window is created.
  const bool always_create_tabbed_browser_;

  // Set of URLs to open in addition to those restored from the session.
  std::vector<GURL> urls_to_open_;

  // Used to get the session.
  CancelableRequestConsumer request_consumer_;

  // Responsible for loading the tabs.
  scoped_ptr<TabLoader> tab_loader_;

  NotificationRegistrar registrar_;
};

}  // namespace

// SessionRestore -------------------------------------------------------------

// static
size_t SessionRestore::num_tabs_to_load_ = 0;

static void Restore(Profile* profile,
                    Browser* browser,
                    bool synchronous,
                    bool clobber_existing_window,
                    bool always_create_tabbed_browser,
                    const std::vector<GURL>& urls_to_open) {
  DCHECK(profile);
  // Always restore from the original profile (incognito profiles have no
  // session service).
  profile = profile->GetOriginalProfile();
  if (!profile->GetSessionService()) {
    NOTREACHED();
    return;
  }
  profile->set_restored_last_session(true);
  // SessionRestoreImpl takes care of deleting itself when done.
  SessionRestoreImpl* restorer =
      new SessionRestoreImpl(profile, browser, synchronous,
                             clobber_existing_window,
                             always_create_tabbed_browser, urls_to_open);
  restorer->Restore();
}

// static
void SessionRestore::RestoreSession(Profile* profile,
                                    Browser* browser,
                                    bool clobber_existing_window,
                                    bool always_create_tabbed_browser,
                                    const std::vector<GURL>& urls_to_open) {
  Restore(profile, browser, false, clobber_existing_window,
          always_create_tabbed_browser, urls_to_open);
}

// static
void SessionRestore::RestoreSessionSynchronously(
    Profile* profile,
    const std::vector<GURL>& urls_to_open) {
  Restore(profile, NULL, true, false, true, urls_to_open);
}
