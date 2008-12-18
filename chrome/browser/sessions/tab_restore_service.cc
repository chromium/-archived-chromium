// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <iterator>

#include "chrome/browser/sessions/tab_restore_service.h"

#include "chrome/browser/browser_list.h"
#include "chrome/browser/navigation_controller.h"
#include "chrome/browser/navigation_entry.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/sessions/session_backend.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/common/scoped_vector.h"
#include "chrome/common/stl_util-inl.h"

using base::Time;

// Entry ----------------------------------------------------------------------

// ID of the next Entry.
static SessionID::id_type next_entry_id = 1;

TabRestoreService::Entry::Entry() : id(next_entry_id++), type(TAB) {}

TabRestoreService::Entry::Entry(Type type) : id(next_entry_id++), type(type) {}

// TabRestoreService ----------------------------------------------------------

// static
const size_t TabRestoreService::kMaxEntries = 10;

// Identifier for commands written to file.
// The ordering in the file is as follows:
// . When the user closes a tab a command of type 
//   kCommandSelectedNavigationInTab is written identifying the tab and
//   the selected index. This is followed by any number of
//   kCommandUpdateTabNavigation commands (1 per navigation entry).
// . When the user closes a window a kCommandSelectedNavigationInTab command
//   is written out and followed by n tab closed sequences (as previoulsy
//   described).
// . When the user restores an entry a command of type kCommandRestoredEntry
//   is written.
static const SessionCommand::id_type kCommandUpdateTabNavigation = 1;
static const SessionCommand::id_type kCommandRestoredEntry = 2;
static const SessionCommand::id_type kCommandWindow = 3;
static const SessionCommand::id_type kCommandSelectedNavigationInTab = 4;

// Number of entries (not commands) before we clobber the file and write
// everything.
static const int kEntriesPerReset = 40;

namespace {

// Payload structures.

typedef int32 RestoredEntryPayload;

// Payload used for the start of a window close.
struct WindowPayload {
  SessionID::id_type window_id;
  int32 selected_tab_index;
  int32 num_tabs;
};

// Payload used for the start of a tab close.
struct SelectedNavigationInTabPayload {
  SessionID::id_type id;
  int32 index;
};

typedef std::map<SessionID::id_type, TabRestoreService::Entry*> IDToEntry;

// If |id_to_entry| contains an entry for |id| the corresponding entry is
// deleted and removed from both |id_to_entry| and |entries|. This is used
// when creating entries from the backend file.
void RemoveEntryByID(SessionID::id_type id,
                     IDToEntry* id_to_entry,
                     std::vector<TabRestoreService::Entry*>* entries) {
  IDToEntry::iterator i = id_to_entry->find(id);
  if (i == id_to_entry->end())
    return;

  entries->erase(std::find(entries->begin(), entries->end(), i->second));
  delete i->second;
  id_to_entry->erase(i);
}

}  // namespace

TabRestoreService::TabRestoreService(Profile* profile)
    : BaseSessionService(BaseSessionService::TAB_RESTORE, profile,
                         std::wstring()),
      load_state_(NOT_LOADED),
      restoring_(false),
      reached_max_(false),
      entries_to_write_(0),
      entries_written_(0) {
}

TabRestoreService::~TabRestoreService() {
  if (backend())
    Save();

  FOR_EACH_OBSERVER(Observer, observer_list_, TabRestoreServiceDestroyed(this));
  STLDeleteElements(&entries_);
  STLDeleteElements(&staging_entries_);
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

  scoped_ptr<Tab> local_tab(new Tab());
  PopulateTabFromController(tab, local_tab.get());
  if (local_tab->navigations.empty())
    return;

  AddEntry(local_tab.release(), true, true);
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
    AddEntry(window, true, true);
  }
}

void TabRestoreService::BrowserClosed(Browser* browser) {
  closing_browsers_.erase(browser);
}

void TabRestoreService::ClearEntries() {
  // Mark all the tabs as closed so that we don't attempt to restore them.
  for (Entries::iterator i = entries_.begin(); i != entries_.end(); ++i)
    ScheduleCommand(CreateRestoredEntryCommand((*i)->id));

  entries_to_write_ = 0;

  // Schedule a pending reset so that we nuke the file on next write.
  set_pending_reset(true);

  // Schedule a command, otherwise if there are no pending commands Save does
  // nothing.
  ScheduleCommand(CreateRestoredEntryCommand(1));

  STLDeleteElements(&entries_);
  NotifyTabsChanged();
}

void TabRestoreService::RestoreMostRecentEntry(Browser* browser) {
  if (entries_.empty())
    return;

  RestoreEntryById(browser, entries_.front()->id, false);
}

void TabRestoreService::RestoreEntryById(Browser* browser,
                                         SessionID::id_type id,
                                         bool replace_existing_tab) {
  Entries::iterator i = GetEntryIteratorById(id);
  if (i == entries_.end()) {
    // Don't hoark here, we allow an invalid id.
    return;
  }

  size_t index = 0;
  for (Entries::iterator j = entries_.begin(); j != i && j != entries_.end();
       ++j, ++index);
  if (static_cast<int>(index) < entries_to_write_)
    entries_to_write_--;

  ScheduleCommand(CreateRestoredEntryCommand(id));

  restoring_ = true;
  Entry* entry = *i;
  entries_.erase(i);
  i = entries_.end();
  if (browser) {  // Browser is null during testing.
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
      Browser* browser = Browser::Create(profile());
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
  }
  delete entry;
  restoring_ = false;
  NotifyTabsChanged();
}

void TabRestoreService::LoadTabsFromLastSession() {
  if (load_state_ != NOT_LOADED || reached_max_)
    return;

  load_state_ = LOADING;

  if (!profile()->restored_last_session() &&
      !profile()->DidLastSessionExitCleanly() &&
      profile()->GetSessionService()) {
    // The previous session crashed and wasn't restored. Load the tabs/windows
    // that were open at the point of crash from the session service.
    profile()->GetSessionService()->GetLastSession(
        &load_consumer_,
        NewCallback(this, &TabRestoreService::OnGotPreviousSession));
  } else {
    load_state_ |= LOADED_LAST_SESSION;
  }

  // Request the tabs closed in the last session. If the last session crashed,
  // this won't contain the tabs/window that were open at the point of the
  // crash (the call to GetLastSession above requests those).
  ScheduleGetLastSessionCommands(
      new InternalGetCommandsRequest(
          NewCallback(this, &TabRestoreService::OnGotLastSessionCommands)),
      &load_consumer_);
}

void TabRestoreService::Save() {
  int to_write_count = std::min(entries_to_write_,
                                static_cast<int>(entries_.size()));
  entries_to_write_ = 0;
  if (entries_written_ + to_write_count > kEntriesPerReset) {
    to_write_count = entries_.size();
    set_pending_reset(true);
  }
  if (to_write_count) {
    // Write the to_write_count most recently added entries out. The most
    // recently added entry is at the front, so we use a reverse iterator to
    // write in the order the entries were added.
    Entries::reverse_iterator i = entries_.rbegin();
    DCHECK(static_cast<size_t>(to_write_count) <= entries_.size());
    std::advance(i, entries_.size() - static_cast<int>(to_write_count));
    for (; i != entries_.rend(); ++i) {
      Entry* entry = *i;
      if (entry->type == TAB) {
        Tab* tab = static_cast<Tab*>(entry);
        int selected_index = GetSelectedNavigationIndexToPersist(*tab);
        if (selected_index != -1)
          ScheduleCommandsForTab(*tab, selected_index);
      } else {
        ScheduleCommandsForWindow(*static_cast<Window*>(entry));
      }
      entries_written_++;
    }
  }
  if (pending_reset())
    entries_written_ = 0;
  BaseSessionService::Save();
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
    tab->navigations[i].SetFromNavigationEntry(*entry);
  }
  tab->current_navigation_index = controller->GetCurrentEntryIndex();
  if (tab->current_navigation_index == -1 && entry_count > 0)
    tab->current_navigation_index = 0;
}

void TabRestoreService::NotifyTabsChanged() {
  FOR_EACH_OBSERVER(Observer, observer_list_, TabRestoreServiceChanged(this));
}

void TabRestoreService::AddEntry(Entry* entry, bool notify, bool to_front) {
  if (to_front)
    entries_.push_front(entry);
  else
    entries_.push_back(entry);
  if (notify)
    PruneAndNotify();
  // Start the save timer, when it fires we'll generate the commands.
  StartSaveTimer();
  entries_to_write_++;
}

void TabRestoreService::PruneAndNotify() {
  while (entries_.size() > kMaxEntries) {
    delete entries_.back();
    entries_.pop_back();
    reached_max_ = true;
  }

  NotifyTabsChanged();
}

TabRestoreService::Entries::iterator TabRestoreService::GetEntryIteratorById(
    SessionID::id_type id) {
  for (Entries::iterator i = entries_.begin(); i != entries_.end(); ++i) {
    if ((*i)->id == id)
      return i;
  }
  return entries_.end();
}

void TabRestoreService::ScheduleCommandsForWindow(const Window& window) {
  DCHECK(!window.tabs.empty());
  int selected_tab = window.selected_tab_index;
  int valid_tab_count = 0;
  int real_selected_tab = selected_tab;
  for (size_t i = 0; i < window.tabs.size(); ++i) {
    if (GetSelectedNavigationIndexToPersist(window.tabs[i]) != -1) {
      valid_tab_count++;
    } else if (static_cast<int>(i) < selected_tab) {
      real_selected_tab--;
    }
  }
  if (valid_tab_count == 0)
    return;  // No tabs to persist.

  ScheduleCommand(
      CreateWindowCommand(window.id,
                          std::min(real_selected_tab, valid_tab_count - 1),
                          valid_tab_count));

  for (size_t i = 0; i < window.tabs.size(); ++i) {
    int selected_index = GetSelectedNavigationIndexToPersist(window.tabs[i]);
    if (selected_index != -1)
      ScheduleCommandsForTab(window.tabs[i], selected_index);
  }
}

void TabRestoreService::ScheduleCommandsForTab(const Tab& tab,
                                               int selected_index) {
  const std::vector<TabNavigation>& navigations = tab.navigations;
  int max_index = static_cast<int>(navigations.size());

  // Determine the first navigation we'll persist.
  int valid_count_before_selected = 0;
  int first_index_to_persist = selected_index;
  for (int i = selected_index - 1; i >= 0 &&
       valid_count_before_selected < max_persist_navigation_count; --i) {
    if (ShouldTrackEntry(navigations[i])) {
      first_index_to_persist = i;
      valid_count_before_selected++;
    }
  }

  // Write the command that identifies the selected tab.
  ScheduleCommand(
      CreateSelectedNavigationInTabCommand(tab.id,
                                           valid_count_before_selected));

  // Then write the navigations.
  for (int i = first_index_to_persist, wrote_count = 0;
       i < max_index && wrote_count < 2 * max_persist_navigation_count; ++i) {
    if (ShouldTrackEntry(navigations[i])) {
      // Creating a NavigationEntry isn't the most efficient way to go about
      // this, but it simplifies the code and makes it less error prone as we
      // add new data to NavigationEntry.
      scoped_ptr<NavigationEntry> entry(
          navigations[i].ToNavigationEntry(wrote_count));
      ScheduleCommand(
          CreateUpdateTabNavigationCommand(kCommandUpdateTabNavigation, tab.id,
                                           wrote_count++, *entry));
    }
  }
}

SessionCommand* TabRestoreService::CreateWindowCommand(SessionID::id_type id,
                                                       int selected_tab_index,
                                                       int num_tabs) {
  WindowPayload payload = { 0 };
  payload.window_id = id;
  payload.selected_tab_index = selected_tab_index;
  payload.num_tabs = num_tabs;

  SessionCommand* command =
      new SessionCommand(kCommandWindow, sizeof(payload));
  memcpy(command->contents(), &payload, sizeof(payload));
  return command;
}

SessionCommand* TabRestoreService::CreateSelectedNavigationInTabCommand(
    SessionID::id_type tab_id,
    int32 index) {
  SelectedNavigationInTabPayload payload = { 0 };
  payload.id = tab_id;
  payload.index = index;
  SessionCommand* command =
      new SessionCommand(kCommandSelectedNavigationInTab, sizeof(payload));
  memcpy(command->contents(), &payload, sizeof(payload));
  return command;
}

SessionCommand* TabRestoreService::CreateRestoredEntryCommand(
    SessionID::id_type entry_id) {
  RestoredEntryPayload payload = entry_id;
  SessionCommand* command =
      new SessionCommand(kCommandRestoredEntry, sizeof(payload));
  memcpy(command->contents(), &payload, sizeof(payload));
  return command;
}

int TabRestoreService::GetSelectedNavigationIndexToPersist(const Tab& tab) {
  const std::vector<TabNavigation>& navigations = tab.navigations;
  int selected_index = tab.current_navigation_index;
  int max_index = static_cast<int>(navigations.size());

  // Find the first navigation to persist. We won't persist the selected
  // navigation if ShouldTrackEntry returns false.
  while (selected_index >= 0 &&
         !ShouldTrackEntry(navigations[selected_index])) {
    selected_index--;
  }

  if (selected_index != -1)
    return selected_index;

  // Couldn't find a navigation to persist going back, go forward.
  selected_index = tab.current_navigation_index + 1;
  while (selected_index < max_index &&
         !ShouldTrackEntry(navigations[selected_index])) {
    selected_index++;
  }

  return (selected_index == max_index) ? -1 : selected_index;
}

void TabRestoreService::OnGotLastSessionCommands(
    Handle handle,
    scoped_refptr<InternalGetCommandsRequest> request) {
  std::vector<Entry*> entries;
  CreateEntriesFromCommands(request, &entries);
  // Closed tabs always go to the end.
  staging_entries_.insert(staging_entries_.end(), entries.begin(),
                          entries.end());
  load_state_ |= LOADED_LAST_TABS;
  LoadStateChanged();
}

void TabRestoreService::CreateEntriesFromCommands(
    scoped_refptr<InternalGetCommandsRequest> request,
    std::vector<Entry*>* loaded_entries) {
  if (request->canceled() || entries_.size() == kMaxEntries)
    return;

  std::vector<SessionCommand*>& commands = request->commands;
  // Iterate through the commands populating entries and id_to_entry.
  ScopedVector<Entry> entries;
  IDToEntry id_to_entry;
  // If non-null we're processing the navigations of this tab.
  Tab* current_tab = NULL;
  // If non-null we're processing the tabs of this window.
  Window* current_window = NULL;
  // If > 0, we've gotten a window command but not all the tabs yet.
  int pending_window_tabs = 0;
  for (std::vector<SessionCommand*>::const_iterator i = commands.begin();
       i != commands.end(); ++i) {
    const SessionCommand& command = *(*i);
    switch (command.id()) {
      case kCommandRestoredEntry: {
        if (pending_window_tabs > 0) {
          // Should never receive a restored command while waiting for all the
          // tabs in a window.
          return;
        }

        current_tab = NULL;
        current_window = NULL;

        RestoredEntryPayload payload;
        if (!command.GetPayload(&payload, sizeof(payload)))
          return;
        RemoveEntryByID(payload, &id_to_entry, &(entries.get()));
        break;
      }

      case kCommandWindow: {
        WindowPayload payload;
        if (pending_window_tabs > 0) {
          // Should never receive a window command while waiting for all the
          // tabs in a window.
          return;
        }

        if (!command.GetPayload(&payload, sizeof(payload)))
          return;

        pending_window_tabs = payload.num_tabs;
        if (pending_window_tabs <= 0) {
          // Should always have at least 1 tab. Likely indicates corruption.
          return;
        }

        RemoveEntryByID(payload.window_id, &id_to_entry, &(entries.get()));

        current_window = new Window();
        current_window->selected_tab_index = payload.selected_tab_index;
        entries->push_back(current_window);
        id_to_entry[payload.window_id] = current_window;
        break;
      }

      case kCommandSelectedNavigationInTab: {
        SelectedNavigationInTabPayload payload;
        if (!command.GetPayload(&payload, sizeof(payload)))
          return;

        if (pending_window_tabs > 0) {
          if (!current_window) {
            // We should have created a window already.
            NOTREACHED();
            return;
          }
          current_window->tabs.resize(current_window->tabs.size() + 1);
          current_tab = &(current_window->tabs.back());
          if (--pending_window_tabs == 0)
            current_window = NULL;
        } else {
          RemoveEntryByID(payload.id, &id_to_entry, &(entries.get()));
          current_tab = new Tab();
          id_to_entry[payload.id] = current_tab;
          entries->push_back(current_tab);
        }
        current_tab->current_navigation_index = payload.index;
        break;
      }

      case kCommandUpdateTabNavigation: {
        if (!current_tab) {
          // Should be in a tab when we get this.
          return;
        }
        current_tab->navigations.resize(current_tab->navigations.size() + 1);
        SessionID::id_type tab_id;
        if (!RestoreUpdateTabNavigationCommand(
            command, &current_tab->navigations.back(), &tab_id)) {
          return;
        }
        break;
      }

      default:
        // Unknown type, usually indicates corruption of file. Ignore it.
        return;
    }
  }

  // If there was corruption some of the entries won't be valid. Prune any
  // entries with no navigations.
  ValidateAndDeleteEmptyEntries(&(entries.get()));

  loaded_entries->swap(entries.get());
}

bool TabRestoreService::ValidateTab(Tab* tab) {
  if (tab->navigations.empty())
    return false;

  tab->current_navigation_index =
      std::max(0, std::min(tab->current_navigation_index,
                           static_cast<int>(tab->navigations.size()) - 1));
  return true;
}

void TabRestoreService::ValidateAndDeleteEmptyEntries(
    std::vector<Entry*>* entries) {
  std::vector<Entry*> valid_entries;
  std::vector<Entry*> invalid_entries;

  size_t max_valid = kMaxEntries - entries_.size();
  // Iterate from the back so that we keep the most recently closed entries.
  for (std::vector<Entry*>::reverse_iterator i = entries->rbegin();
       i != entries->rend(); ++i) {
    bool valid_entry = false;
    if (valid_entries.size() != max_valid) {
      if ((*i)->type == TAB) {
        Tab* tab = static_cast<Tab*>(*i);
        if (ValidateTab(tab))
          valid_entry = true;
      } else {
        Window* window = static_cast<Window*>(*i);
        for (std::vector<Tab>::iterator tab_i = window->tabs.begin();
             tab_i != window->tabs.end();) {
          if (!ValidateTab(&(*tab_i)))
            tab_i = window->tabs.erase(tab_i);
          else
            ++tab_i;
        }
        if (!window->tabs.empty()) {
          window->selected_tab_index =
              std::max(0, std::min(window->selected_tab_index,
                                   static_cast<int>(window->tabs.size() - 1)));
          valid_entry = true;
        }
      }
    }
    if (valid_entry)
      valid_entries.push_back(*i);
    else
      invalid_entries.push_back(*i);
  }
  // NOTE: at this point the entries are ordered with newest at the front.
  entries->swap(valid_entries);

  // Delete the remaining entries.
  STLDeleteElements(&invalid_entries);
}

void TabRestoreService::OnGotPreviousSession(
    Handle handle,
    std::vector<SessionWindow*>* windows) {
  std::vector<Entry*> entries;
  CreateEntriesFromWindows(windows, &entries);
  // Previous session tabs go first.
  staging_entries_.insert(staging_entries_.begin(), entries.begin(),
                          entries.end());
  load_state_ |= LOADED_LAST_SESSION;
  LoadStateChanged();
}

void TabRestoreService::CreateEntriesFromWindows(
    std::vector<SessionWindow*>* windows,
    std::vector<Entry*>* entries) {
  for (size_t i = 0; i < windows->size(); ++i) {
    scoped_ptr<Window> window(new Window());
    if (ConvertSessionWindowToWindow((*windows)[i], window.get()))
      entries->push_back(window.release());
  }
}

bool TabRestoreService::ConvertSessionWindowToWindow(
    SessionWindow* session_window,
    Window* window) {
  for (size_t i = 0; i < session_window->tabs.size(); ++i) {
    if (!session_window->tabs[i]->navigations.empty()) {
      window->tabs.resize(window->tabs.size() + 1);
      Tab& tab = window->tabs.back();
      tab.navigations.swap(session_window->tabs[i]->navigations);
      tab.current_navigation_index =
          session_window->tabs[i]->current_navigation_index;
    }
  }
  if (window->tabs.empty())
    return false;

  window->selected_tab_index =
      std::min(session_window->selected_tab_index,
               static_cast<int>(window->tabs.size() - 1));
  return true;
}

void TabRestoreService::LoadStateChanged() {
  if ((load_state_ & (LOADED_LAST_TABS | LOADED_LAST_SESSION)) !=
      (LOADED_LAST_TABS | LOADED_LAST_SESSION)) {
    // Still waiting on previous session or previous tabs.
    return;
  }

  // We're done loading.
  load_state_ ^= LOADING;

  if (staging_entries_.empty() || reached_max_) {
    STLDeleteElements(&staging_entries_);
    return;
  }

  if (staging_entries_.size() + entries_.size() > kMaxEntries) {
    // If we add all the staged entries we'll end up with more than
    // kMaxEntries. Delete entries such that we only end up with
    // at most kMaxEntries.
    DCHECK(entries_.size() < kMaxEntries);
    STLDeleteContainerPointers(
        staging_entries_.begin() + (kMaxEntries - entries_.size()),
        staging_entries_.end());
    staging_entries_.erase(
        staging_entries_.begin() + (kMaxEntries - entries_.size()),
        staging_entries_.end());
  }

  // And add them.
  for (size_t i = 0; i < staging_entries_.size(); ++i)
    AddEntry(staging_entries_[i], false, false);

  // AddEntry takes ownership of the entry, need to clear out entries so that
  // it doesn't delete them.
  staging_entries_.clear();

  // Make it so we rewrite all the tabs. We need to do this otherwise we won't
  // correctly write out the entries when Save is invoked (Save starts from
  // the front, not the end and we just added the entries to the end).
  entries_to_write_ = staging_entries_.size();

  PruneAndNotify();
}
