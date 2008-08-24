// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_restore_service.h"

#include "chrome/browser/profile.h"
#include "chrome/browser/navigation_controller.h"
#include "chrome/browser/navigation_entry.h"

// HistoricalTab --------------------------------------------------------------

// ID of the next HistoricalTab.
static int next_historical_tab_id = 1;

TabRestoreService::HistoricalTab::HistoricalTab()
    : close_time(Time::Now()),
      from_last_session(false),
      current_navigation_index(-1),
      id(next_historical_tab_id++) {
}

// TabRestoreService ----------------------------------------------------------

// Max number of tabs we'll keep around.
static const size_t kMaxTabs = 10;

// Amount of time from when the session starts and when we'll allow loading of
// the last sessions tabs.
static const int kLoadFromLastSessionMS = 600000;

TabRestoreService::TabRestoreService(Profile* profile)
    : profile_(profile),
      loaded_last_session_(false) {
}

TabRestoreService::~TabRestoreService() {
  FOR_EACH_OBSERVER(Observer, observer_list_, TabRestoreServiceDestroyed(this));
}

void TabRestoreService::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void TabRestoreService::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void TabRestoreService::LoadPreviousSessionTabs() {
  if (!WillLoadPreviousSessionTabs() || IsLoadingPreviousSessionTabs())
    return;

  profile_->GetSessionService()->GetLastSession(
      &cancelable_consumer_,
      NewCallback(this, &TabRestoreService::OnGotLastSession));
}

bool TabRestoreService::IsLoadingPreviousSessionTabs() {
  return cancelable_consumer_.HasPendingRequests();
}

bool TabRestoreService::WillLoadPreviousSessionTabs() {
  return (!loaded_last_session_ && tabs_.size() < kMaxTabs &&
          (Time::Now() - profile_->GetStartTime()).InMilliseconds() <
          kLoadFromLastSessionMS);
}

void TabRestoreService::CreateHistoricalTab(NavigationController* tab) {
  tabs_.push_front(HistoricalTab());

  PopulateTabFromController(tab, &(tabs_.front()));

  while (tabs_.size() > kMaxTabs)
    tabs_.pop_back();

  NotifyTabsChanged();
}

void TabRestoreService::RemoveHistoricalTabById(int id) {
  for (Tabs::iterator i = tabs_.begin(); i != tabs_.end(); ++i) {
    if (i->id == id) {
      tabs_.erase(i);
      NotifyTabsChanged();
      return;
    }
  }
  // Don't hoark here, we allow an invalid id.
}

void TabRestoreService::ClearHistoricalTabs() {
  tabs_.clear();
  NotifyTabsChanged();
}

void TabRestoreService::OnGotLastSession(SessionService::Handle handle,
                                         std::vector<SessionWindow*>* windows) {
  DCHECK(!loaded_last_session_);
  loaded_last_session_ = true;

  if (tabs_.size() == kMaxTabs)
    return;

  AddHistoricalTabs(windows);

  NotifyTabsChanged();
}

void TabRestoreService::AddHistoricalTabs(
    std::vector<SessionWindow*>* windows) {
  // First pass, extract the selected tabs in each window.
  for (size_t i = 0; i < windows->size(); ++i) {
    SessionWindow* window = (*windows)[i];
    if (window->type == BrowserType::TABBED_BROWSER) {
      DCHECK(window->selected_tab_index >= 0 &&
             window->selected_tab_index <
             static_cast<int>(window->tabs.size()));
      AppendHistoricalTabFromSessionTab(
          window->tabs[window->selected_tab_index]);
      if (tabs_.size() == kMaxTabs)
        return;
    }
  }

  // Second pass, extract the non-selected tabs.
  for (size_t window_index = 0; window_index < windows->size();
       ++window_index) {
    SessionWindow* window = (*windows)[window_index];
    if (window->type != BrowserType::TABBED_BROWSER)
      continue; // Ignore popups.

    for (size_t tab_index = 0; tab_index < window->tabs.size(); ++tab_index) {
      if (tab_index == window->selected_tab_index)
        continue; // Pass one took care of this tab.
      AppendHistoricalTabFromSessionTab(window->tabs[tab_index]);
      if (tabs_.size() == kMaxTabs)
        return;
    }
  }
}

void TabRestoreService::AppendHistoricalTabFromSessionTab(
    SessionTab* tab) {
  tabs_.push_back(HistoricalTab());
  PopulateTabFromSessionTab(tab, &(tabs_.back()));
}

void TabRestoreService::PopulateTabFromController(
    NavigationController* controller,
    HistoricalTab* tab) {
  const int pending_index = controller->GetPendingEntryIndex();
  int entry_count = controller->GetEntryCount();
  if (entry_count == 0 && pending_index == 0)
    entry_count++;
  tab->navigations.resize(static_cast<int>(entry_count));
  for (int i = 0; i < entry_count; ++i) {
    NavigationEntry* entry = (i == pending_index) ?
        controller->GetPendingEntry() : controller->GetEntryAtIndex(i);
    TabNavigation& tab_nav = tab->navigations[i];
    tab_nav.index = i;
    tab_nav.url = entry->GetDisplayURL();
    tab_nav.title = entry->GetTitle();
    tab_nav.state = entry->GetContentState();
    tab_nav.transition = entry->GetTransitionType();
    tab_nav.type_mask = entry->HasPostData() ? TabNavigation::HAS_POST_DATA : 0;
  }
  tab->current_navigation_index = controller->GetCurrentEntryIndex();
  if (tab->current_navigation_index == -1 && entry_count > 0)
    tab->current_navigation_index = 0;
}

void TabRestoreService::PopulateTabFromSessionTab(
    SessionTab* session_tab,
    HistoricalTab* tab) {
  tab->navigations.swap(session_tab->navigations);
  tab->from_last_session = true;
  tab->current_navigation_index = session_tab->current_navigation_index;
}

void TabRestoreService::NotifyTabsChanged() {
  FOR_EACH_OBSERVER(Observer, observer_list_, TabRestoreServiceChanged(this));
}

