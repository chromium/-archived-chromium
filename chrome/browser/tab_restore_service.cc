// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_restore_service.h"

#include "chrome/browser/browser_list.h"
#include "chrome/browser/navigation_controller.h"
#include "chrome/browser/navigation_entry.h"
#include "chrome/browser/profile.h"
#include "chrome/common/stl_util-inl.h"

using base::Time;

// Entry ----------------------------------------------------------------------

// ID of the next Entry.
static int next_entry_id = 1;

TabRestoreService::Entry::Entry() : id(next_entry_id++), type(TAB) {}

TabRestoreService::Entry::Entry(Type type) : id(next_entry_id++), type(type) {}

// TabRestoreService ----------------------------------------------------------

// Max number of entries we'll keep around.
static const size_t kMaxEntries = 10;

TabRestoreService::TabRestoreService(Profile* profile)
    : profile_(profile),
      loaded_last_session_(false),
      restoring_(false) {
}

TabRestoreService::~TabRestoreService() {
  FOR_EACH_OBSERVER(Observer, observer_list_, TabRestoreServiceDestroyed(this));
  STLDeleteElements(&entries_);
}

void TabRestoreService::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void TabRestoreService::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void TabRestoreService::CreateHistoricalTab(NavigationController* tab) {
  if (restoring_)
    return;

  Browser* browser = Browser::GetBrowserForController(tab, NULL);
  if (closing_browsers_.find(browser) != closing_browsers_.end())
    return;

  Tab* local_tab = new Tab();
  PopulateTabFromController(tab, local_tab);
  entries_.push_front(local_tab);

  PruneAndNotify();
}

void TabRestoreService::BrowserClosing(Browser* browser) {
  if (browser->type() != Browser::TYPE_NORMAL ||
      browser->tab_count() == 0)
    return;

  closing_browsers_.insert(browser);

  Window* window = new Window();
  window->selected_tab_index = browser->selected_index();
  window->tabs.resize(browser->tab_count());
  size_t entry_index = 0;
  for (int tab_index = 0; tab_index < browser->tab_count(); ++tab_index) {
    PopulateTabFromController(
        browser->GetTabContentsAt(tab_index)->controller(),
        &(window->tabs[entry_index]));
    if (window->tabs[entry_index].navigations.empty())
      window->tabs.erase(window->tabs.begin() + entry_index);
    else
      entry_index++;
  }
  if (window->tabs.empty()) {
    delete window;
    window = NULL;
  } else {
    entries_.push_front(window);
    PruneAndNotify();
  }
}

void TabRestoreService::BrowserClosed(Browser* browser) {
  closing_browsers_.erase(browser);
}

void TabRestoreService::ClearEntries() {
  STLDeleteElements(&entries_);
  NotifyTabsChanged();
}

void TabRestoreService::RestoreMostRecentEntry(Browser* browser) {
  if (entries_.empty())
    return;

  RestoreEntryById(browser, entries_.front()->id, false);
}

void TabRestoreService::RestoreEntryById(Browser* browser,
                                         int id,
                                         bool replace_existing_tab) {
  Entries::iterator i = GetEntryIteratorById(id);
  if (i == entries_.end()) {
    // Don't hoark here, we allow an invalid id.
    return;
  }

  restoring_ = true;
  Entry* entry = *i;
  entries_.erase(i);
  i = entries_.end();
  if (entry->type == TAB) {
    Tab* tab = static_cast<Tab*>(entry);
    if (replace_existing_tab) {
      browser->ReplaceRestoredTab(tab->navigations,
                                  tab->current_navigation_index);
    } else {
      browser->AddRestoredTab(tab->navigations, browser->tab_count(),
                              tab->current_navigation_index, true);
    }
  } else if (entry->type == WINDOW) {
    const Window* window = static_cast<Window*>(entry);
    Browser* browser = Browser::Create(profile_);
    for (size_t tab_i = 0; tab_i < window->tabs.size(); ++tab_i) {
      const Tab& tab = window->tabs[tab_i];
      NavigationController* restored_controller =
          browser->AddRestoredTab(tab.navigations, browser->tab_count(),
                                  tab.current_navigation_index,
                                  (tab_i == window->selected_tab_index));
      if (restored_controller)
        restored_controller->LoadIfNecessary();
    }
    browser->window()->Show();
  } else {
    NOTREACHED();
  }
  delete entry;
  restoring_ = false;
  NotifyTabsChanged();
}

void TabRestoreService::PopulateTabFromController(
    NavigationController* controller,
    Tab* tab) {
  const int pending_index = controller->GetPendingEntryIndex();
  int entry_count = controller->GetEntryCount();
  if (entry_count == 0 && pending_index == 0)
    entry_count++;
  tab->navigations.resize(static_cast<int>(entry_count));
  for (int i = 0; i < entry_count; ++i) {
    NavigationEntry* entry = (i == pending_index) ?
        controller->GetPendingEntry() : controller->GetEntryAtIndex(i);
    TabNavigation& tab_nav = tab->navigations[i];
    tab_nav.url = entry->display_url();
    tab_nav.referrer = entry->referrer();
    tab_nav.title = entry->title();
    tab_nav.state = entry->content_state();
    tab_nav.transition = entry->transition_type();
    tab_nav.type_mask = entry->has_post_data() ?
        TabNavigation::HAS_POST_DATA : 0;
  }
  tab->current_navigation_index = controller->GetCurrentEntryIndex();
  if (tab->current_navigation_index == -1 && entry_count > 0)
    tab->current_navigation_index = 0;
}

void TabRestoreService::NotifyTabsChanged() {
  FOR_EACH_OBSERVER(Observer, observer_list_, TabRestoreServiceChanged(this));
}

void TabRestoreService::PruneAndNotify() {
  while (entries_.size() > kMaxEntries) {
    delete entries_.back();
    entries_.pop_back();
  }

  NotifyTabsChanged();
}

TabRestoreService::Entries::iterator TabRestoreService::GetEntryIteratorById(
    int id) {
  for (Entries::iterator i = entries_.begin(); i != entries_.end(); ++i) {
    if ((*i)->id == id)
      return i;
  }
  return entries_.end();
}
