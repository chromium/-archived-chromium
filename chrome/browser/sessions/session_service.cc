// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/session_service.h"

#include <limits>

#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/pickle.h"
#include "base/thread.h"
#include "chrome/browser/browser_init.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/navigation_controller.h"
#include "chrome/browser/navigation_entry.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/session_startup_pref.h"
#include "chrome/browser/sessions/session_backend.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/sessions/session_types.h"
#include "chrome/browser/tab_contents.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_types.h"
#include "chrome/common/scoped_vector.h"
#include "chrome/common/win_util.h"

using base::Time;

// Identifier for commands written to file.
static const SessionCommand::id_type kCommandSetTabWindow = 0;
// kCommandSetWindowBounds is no longer used (it's superseded by
// kCommandSetWindowBounds2). I leave it here to document what it was.
// static const SessionCommand::id_type kCommandSetWindowBounds = 1;
static const SessionCommand::id_type kCommandSetTabIndexInWindow = 2;
static const SessionCommand::id_type kCommandTabClosed = 3;
static const SessionCommand::id_type kCommandWindowClosed = 4;
static const SessionCommand::id_type
    kCommandTabNavigationPathPrunedFromBack = 5;
static const SessionCommand::id_type kCommandUpdateTabNavigation = 6;
static const SessionCommand::id_type kCommandSetSelectedNavigationIndex = 7;
static const SessionCommand::id_type kCommandSetSelectedTabInIndex = 8;
static const SessionCommand::id_type kCommandSetWindowType = 9;
static const SessionCommand::id_type kCommandSetWindowBounds2 = 10;
static const SessionCommand::id_type
    kCommandTabNavigationPathPrunedFromFront = 11;

// Every kWritesPerReset commands triggers recreating the file.
static const int kWritesPerReset = 250;

namespace {

// The callback from GetLastSession is internally routed to SessionService
// first and then the caller. This is done so that the SessionWindows can be
// recreated from the SessionCommands and the SessionWindows passed to the
// caller. The following class is used for this.
class InternalLastSessionRequest :
    public BaseSessionService::InternalGetCommandsRequest {
 public:
  InternalLastSessionRequest(
      CallbackType* callback,
      SessionService::LastSessionCallback* real_callback)
      : BaseSessionService::InternalGetCommandsRequest(callback),
        real_callback(real_callback) {
  }

  // The callback supplied to GetLastSession.
  scoped_ptr<SessionService::LastSessionCallback> real_callback;

 private:
  DISALLOW_COPY_AND_ASSIGN(InternalLastSessionRequest);
};

// Various payload structures.
struct ClosedPayload {
  SessionID::id_type id;
  int64 close_time;
};

struct WindowBoundsPayload2 {
  SessionID::id_type window_id;
  int32 x;
  int32 y;
  int32 w;
  int32 h;
  bool is_maximized;
};

struct IDAndIndexPayload {
  SessionID::id_type id;
  int32 index;
};

typedef IDAndIndexPayload TabIndexInWindowPayload;

typedef IDAndIndexPayload TabNavigationPathPrunedFromBackPayload;

typedef IDAndIndexPayload SelectedNavigationIndexPayload;

typedef IDAndIndexPayload SelectedTabInIndexPayload;

typedef IDAndIndexPayload WindowTypePayload;

typedef IDAndIndexPayload TabNavigationPathPrunedFromFrontPayload;

}  // namespace

// SessionService -------------------------------------------------------------

SessionService::SessionService(Profile* profile)
    : BaseSessionService(SESSION_RESTORE, profile, std::wstring()),
      has_open_tabbed_browsers_(false),
      move_on_new_browser_(false) {
  Init();
}

SessionService::SessionService(const std::wstring& save_path)
    : BaseSessionService(SESSION_RESTORE, NULL, save_path),
      has_open_tabbed_browsers_(false),
      move_on_new_browser_(false) {
  Init();
}

SessionService::~SessionService() {
  Save();

  // Unregister our notifications.
  NotificationService::current()->RemoveObserver(
      this, NOTIFY_TAB_PARENTED, NotificationService::AllSources());
  NotificationService::current()->RemoveObserver(
      this, NOTIFY_TAB_CLOSED, NotificationService::AllSources());
  NotificationService::current()->RemoveObserver(
      this, NOTIFY_NAV_LIST_PRUNED, NotificationService::AllSources());
  NotificationService::current()->RemoveObserver(
      this, NOTIFY_NAV_ENTRY_CHANGED, NotificationService::AllSources());
  NotificationService::current()->RemoveObserver(
      this, NOTIFY_NAV_ENTRY_COMMITTED, NotificationService::AllSources());
  NotificationService::current()->RemoveObserver(
      this, NOTIFY_BROWSER_OPENED, NotificationService::AllSources());
}

void SessionService::ResetFromCurrentBrowsers() {
  ScheduleReset();
}

void SessionService::MoveCurrentSessionToLastSession() {
  pending_tab_close_ids_.clear();
  window_closing_ids_.clear();
  pending_window_close_ids_.clear();

  Save();

  if (!backend_thread()) {
    backend()->MoveCurrentSessionToLastSession();
  } else {
    backend_thread()->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
        backend(), &SessionBackend::MoveCurrentSessionToLastSession));
  }
}

void SessionService::SetTabWindow(const SessionID& window_id,
                                  const SessionID& tab_id) {
  if (!ShouldTrackChangesToWindow(window_id))
    return;

  ScheduleCommand(CreateSetTabWindowCommand(window_id, tab_id));
}

void SessionService::SetWindowBounds(const SessionID& window_id,
                                     const gfx::Rect& bounds,
                                     bool is_maximized) {
  if (!ShouldTrackChangesToWindow(window_id))
    return;

  ScheduleCommand(CreateSetWindowBoundsCommand(window_id, bounds,
                                               is_maximized));
}

void SessionService::SetTabIndexInWindow(const SessionID& window_id,
                                         const SessionID& tab_id,
                                         int new_index) {
  if (!ShouldTrackChangesToWindow(window_id))
    return;

  ScheduleCommand(CreateSetTabIndexInWindowCommand(tab_id, new_index));
}

void SessionService::TabClosed(const SessionID& window_id,
                               const SessionID& tab_id) {
  if (!ShouldTrackChangesToWindow(window_id))
    return;

  IdToRange::iterator i = tab_to_available_range_.find(tab_id.id());
  if (i != tab_to_available_range_.end())
    tab_to_available_range_.erase(i);

  if (find(pending_window_close_ids_.begin(), pending_window_close_ids_.end(),
           window_id.id()) != pending_window_close_ids_.end()) {
    // Tab is in last window. Don't commit it immediately, instead add it to the
    // list of tabs to close. If the user creates another window, the close is
    // committed.
    pending_tab_close_ids_.insert(tab_id.id());
  } else if (find(window_closing_ids_.begin(), window_closing_ids_.end(),
                  window_id.id()) != window_closing_ids_.end() ||
             !IsOnlyOneTabLeft()) {
    // Tab closure is the result of a window close (and it isn't the last
    // window), or closing a tab and there are other windows/tabs open. Mark the
    // tab as closed.
    ScheduleCommand(CreateTabClosedCommand(tab_id.id()));
  } else {
    // User closed the last tab in the last tabbed browser. Don't mark the
    // tab closed.
    pending_tab_close_ids_.insert(tab_id.id());
    has_open_tabbed_browsers_ = false;
  }
}

void SessionService::WindowClosing(const SessionID& window_id) {
  if (!ShouldTrackChangesToWindow(window_id))
    return;

  // The window is about to close. If there are other tabbed browsers with the
  // same original profile commit the close immediately.
  //
  // NOTE: if the user chooses the exit menu item session service is destroyed
  // and this code isn't hit.
  if (has_open_tabbed_browsers_) {
    // Closing a window can never make has_open_tabbed_browsers_ go from false
    // to true, so only update it if already true.
    has_open_tabbed_browsers_ = HasOpenTabbedBrowsers(window_id);
  }
  if (!has_open_tabbed_browsers_)
    pending_window_close_ids_.insert(window_id.id());
  else
    window_closing_ids_.insert(window_id.id());
}

void SessionService::WindowClosed(const SessionID& window_id) {
  if (!ShouldTrackChangesToWindow(window_id))
    return;

  windows_tracking_.erase(window_id.id());

  if (window_closing_ids_.find(window_id.id()) != window_closing_ids_.end()) {
    window_closing_ids_.erase(window_id.id());
    ScheduleCommand(CreateWindowClosedCommand(window_id.id()));
  } else if (pending_window_close_ids_.find(window_id.id()) ==
             pending_window_close_ids_.end()) {
    // We'll hit this if user closed the last tab in a window.
    has_open_tabbed_browsers_ = HasOpenTabbedBrowsers(window_id);
    if (!has_open_tabbed_browsers_)
      pending_window_close_ids_.insert(window_id.id());
    else
      ScheduleCommand(CreateWindowClosedCommand(window_id.id()));
  }
}

void SessionService::SetWindowType(const SessionID& window_id,
                                   Browser::Type type) {
  if (!should_track_changes_for_browser_type(type))
    return;

  windows_tracking_.insert(window_id.id());

  // The user created a new tabbed browser with our profile. Commit any
  // pending closes.
  CommitPendingCloses();

  has_open_tabbed_browsers_ = true;
  move_on_new_browser_ = true;

  ScheduleCommand(CreateSetWindowTypeCommand(window_id, type));
}

void SessionService::TabNavigationPathPrunedFromBack(const SessionID& window_id,
                                                     const SessionID& tab_id,
                                                     int count) {
  if (!ShouldTrackChangesToWindow(window_id))
    return;

  TabNavigationPathPrunedFromBackPayload payload = { 0 };
  payload.id = tab_id.id();
  payload.index = count;
  SessionCommand* command =
      new SessionCommand(kCommandTabNavigationPathPrunedFromBack,
                         sizeof(payload));
  memcpy(command->contents(), &payload, sizeof(payload));
  ScheduleCommand(command);
}

void SessionService::TabNavigationPathPrunedFromFront(
    const SessionID& window_id,
    const SessionID& tab_id,
    int count) {
  if (!ShouldTrackChangesToWindow(window_id))
    return;

  // Update the range of indices.
  if (tab_to_available_range_.find(tab_id.id()) !=
      tab_to_available_range_.end()) {
    std::pair<int, int>& range = tab_to_available_range_[tab_id.id()];
    range.first = std::max(0, range.first - count);
    range.second = std::max(0, range.second - count);
  }

  TabNavigationPathPrunedFromFrontPayload payload = { 0 };
  payload.id = tab_id.id();
  payload.index = count;
  SessionCommand* command =
      new SessionCommand(kCommandTabNavigationPathPrunedFromFront,
                         sizeof(payload));
  memcpy(command->contents(), &payload, sizeof(payload));
  ScheduleCommand(command);
}

void SessionService::UpdateTabNavigation(const SessionID& window_id,
                                         const SessionID& tab_id,
                                         int index,
                                         const NavigationEntry& entry) {
  if (!ShouldTrackEntry(entry) || !ShouldTrackChangesToWindow(window_id))
    return;

  if (tab_to_available_range_.find(tab_id.id()) !=
      tab_to_available_range_.end()) {
    std::pair<int, int>& range = tab_to_available_range_[tab_id.id()];
    range.first = std::min(index, range.first);
    range.second = std::max(index, range.second);
  }
  ScheduleCommand(CreateUpdateTabNavigationCommand(kCommandUpdateTabNavigation,
                                                   tab_id.id(), index, entry));
}

void SessionService::TabRestored(NavigationController* controller) {
  if (!ShouldTrackChangesToWindow(controller->window_id()))
    return;

  BuildCommandsForTab(controller->window_id(), controller, -1,
                      &pending_commands(), NULL);
  StartSaveTimer();
}

void SessionService::SetSelectedNavigationIndex(const SessionID& window_id,
                                                const SessionID& tab_id,
                                                int index) {
  if (!ShouldTrackChangesToWindow(window_id))
    return;

  if (tab_to_available_range_.find(tab_id.id()) !=
      tab_to_available_range_.end()) {
    if (index < tab_to_available_range_[tab_id.id()].first ||
        index > tab_to_available_range_[tab_id.id()].second) {
      // The new index is outside the range of what we've archived, schedule
      // a reset.
      ResetFromCurrentBrowsers();
      return;
    }
  }
  ScheduleCommand(CreateSetSelectedNavigationIndexCommand(tab_id, index));
}

void SessionService::SetSelectedTabInWindow(const SessionID& window_id,
                                            int index) {
  if (!ShouldTrackChangesToWindow(window_id))
    return;

  ScheduleCommand(CreateSetSelectedTabInWindow(window_id, index));
}

SessionService::Handle SessionService::GetLastSession(
    CancelableRequestConsumerBase* consumer,
    LastSessionCallback* callback) {
  return ScheduleGetLastSessionCommands(
      new InternalLastSessionRequest(
          NewCallback(this, &SessionService::OnGotLastSessionCommands),
          callback), consumer);
}

void SessionService::Init() {
  // Register for the notifications we're interested in.
  NotificationService::current()->AddObserver(
      this, NOTIFY_TAB_PARENTED, NotificationService::AllSources());
  NotificationService::current()->AddObserver(
      this, NOTIFY_TAB_CLOSED, NotificationService::AllSources());
  NotificationService::current()->AddObserver(
      this, NOTIFY_NAV_LIST_PRUNED, NotificationService::AllSources());
  NotificationService::current()->AddObserver(
      this, NOTIFY_NAV_ENTRY_CHANGED, NotificationService::AllSources());
  NotificationService::current()->AddObserver(
      this, NOTIFY_NAV_ENTRY_COMMITTED, NotificationService::AllSources());
  NotificationService::current()->AddObserver(
      this, NOTIFY_BROWSER_OPENED, NotificationService::AllSources());
}

void SessionService::Observe(NotificationType type,
                             const NotificationSource& source,
                             const NotificationDetails& details) {
  // All of our messages have the NavigationController as the source.
  switch (type) {
    case NOTIFY_BROWSER_OPENED: {
      Browser* browser = Source<Browser>(source).ptr();
      if (browser->profile() != profile() ||
          !should_track_changes_for_browser_type(browser->type())) {
        return;
      }

      if (!has_open_tabbed_browsers_ && !BrowserInit::InProcessStartup()) {
        // We're going from no tabbed browsers to a tabbed browser (and not in
        // process startup), restore the last session.
        if (move_on_new_browser_) {
          // Make the current session the last.
          MoveCurrentSessionToLastSession();
          move_on_new_browser_ = false;
        }
        SessionStartupPref pref = SessionStartupPref::GetStartupPref(profile());
        if (pref.type == SessionStartupPref::LAST) {
          SessionRestore::RestoreSession(
              profile(), browser, false, false, std::vector<GURL>());
        }
      }
      SetWindowType(browser->session_id(), browser->type());
      break;
    }

    case NOTIFY_TAB_PARENTED: {
      NavigationController* controller =
          Source<NavigationController>(source).ptr();
      SetTabWindow(controller->window_id(), controller->session_id());
      break;
    }

    case NOTIFY_TAB_CLOSED: {
      NavigationController* controller =
          Source<NavigationController>(source).ptr();
      TabClosed(controller->window_id(), controller->session_id());
      break;
    }

    case NOTIFY_NAV_LIST_PRUNED: {
      NavigationController* controller =
          Source<NavigationController>(source).ptr();
      Details<NavigationController::PrunedDetails> pruned_details(details);
      if (pruned_details->from_front) {
        TabNavigationPathPrunedFromFront(controller->window_id(),
                                         controller->session_id(),
                                         pruned_details->count);
      } else {
        TabNavigationPathPrunedFromBack(controller->window_id(),
                                        controller->session_id(),
                                        controller->GetEntryCount());
      }
      break;
    }

    case NOTIFY_NAV_ENTRY_CHANGED: {
      NavigationController* controller =
          Source<NavigationController>(source).ptr();
      Details<NavigationController::EntryChangedDetails> changed(details);
      UpdateTabNavigation(controller->window_id(), controller->session_id(),
                          changed->index, *changed->changed_entry);
      break;
    }

    case NOTIFY_NAV_ENTRY_COMMITTED: {
      NavigationController* controller =
          Source<NavigationController>(source).ptr();
      int current_entry_index = controller->GetCurrentEntryIndex();
      SetSelectedNavigationIndex(controller->window_id(),
                                 controller->session_id(),
                                 current_entry_index);
      UpdateTabNavigation(controller->window_id(), controller->session_id(),
                          current_entry_index,
                          *controller->GetEntryAtIndex(current_entry_index));
      break;
    }

    default:
      NOTREACHED();
  }
}

SessionCommand* SessionService::CreateSetSelectedTabInWindow(
    const SessionID& window_id,
    int index) {
  SelectedTabInIndexPayload payload = { 0 };
  payload.id = window_id.id();
  payload.index = index;
  SessionCommand* command = new SessionCommand(kCommandSetSelectedTabInIndex,
                                 sizeof(payload));
  memcpy(command->contents(), &payload, sizeof(payload));
  return command;
}

SessionCommand* SessionService::CreateSetTabWindowCommand(
    const SessionID& window_id,
    const SessionID& tab_id) {
  SessionID::id_type payload[] = { window_id.id(), tab_id.id() };
  SessionCommand* command =
      new SessionCommand(kCommandSetTabWindow, sizeof(payload));
  memcpy(command->contents(), payload, sizeof(payload));
  return command;
}

SessionCommand* SessionService::CreateSetWindowBoundsCommand(
    const SessionID& window_id,
    const gfx::Rect& bounds,
    bool is_maximized) {
  WindowBoundsPayload2 payload = { 0 };
  payload.window_id = window_id.id();
  payload.x = bounds.x();
  payload.y = bounds.y();
  payload.w = bounds.width();
  payload.h = bounds.height();
  payload.is_maximized = is_maximized;
  SessionCommand* command = new SessionCommand(kCommandSetWindowBounds2,
                                               sizeof(payload));
  memcpy(command->contents(), &payload, sizeof(payload));
  return command;
}

SessionCommand* SessionService::CreateSetTabIndexInWindowCommand(
    const SessionID& tab_id,
    int new_index) {
  TabIndexInWindowPayload payload = { 0 };
  payload.id = tab_id.id();
  payload.index = new_index;
  SessionCommand* command =
      new SessionCommand(kCommandSetTabIndexInWindow, sizeof(payload));
  memcpy(command->contents(), &payload, sizeof(payload));
  return command;
}

SessionCommand* SessionService::CreateTabClosedCommand(
    const SessionID::id_type tab_id) {
  ClosedPayload payload;
  // Because of what appears to be a compiler bug setting payload to {0} doesn't
  // set the padding to 0, resulting in Purify reporting an UMR when we write
  // the structure to disk. To avoid this we explicitly memset the struct.
  memset(&payload, 0, sizeof(payload));
  payload.id = tab_id;
  payload.close_time = Time::Now().ToInternalValue();
  SessionCommand* command =
      new SessionCommand(kCommandTabClosed, sizeof(payload));
  memcpy(command->contents(), &payload, sizeof(payload));
  return command;
}

SessionCommand* SessionService::CreateWindowClosedCommand(
    const SessionID::id_type window_id) {
  ClosedPayload payload;
  // See comment in CreateTabClosedCommand as to why we do this.
  memset(&payload, 0, sizeof(payload));
  payload.id = window_id;
  payload.close_time = Time::Now().ToInternalValue();
  SessionCommand* command =
      new SessionCommand(kCommandWindowClosed, sizeof(payload));
  memcpy(command->contents(), &payload, sizeof(payload));
  return command;
}

SessionCommand* SessionService::CreateSetSelectedNavigationIndexCommand(
    const SessionID& tab_id,
    int index) {
  SelectedNavigationIndexPayload payload = { 0 };
  payload.id = tab_id.id();
  payload.index = index;
  SessionCommand* command = new SessionCommand(
      kCommandSetSelectedNavigationIndex, sizeof(payload));
  memcpy(command->contents(), &payload, sizeof(payload));
  return command;
}

SessionCommand* SessionService::CreateSetWindowTypeCommand(
    const SessionID& window_id,
    Browser::Type type) {
      WindowTypePayload payload = { 0 };
  payload.id = window_id.id();
  payload.index = static_cast<int32>(type);
  SessionCommand* command = new SessionCommand(
      kCommandSetWindowType, sizeof(payload));
  memcpy(command->contents(), &payload, sizeof(payload));
  return command;
}

void SessionService::OnGotLastSessionCommands(
    Handle handle,
    scoped_refptr<InternalGetCommandsRequest> request) {
  if (request->canceled())
    return;
  ScopedVector<SessionWindow> valid_windows;
  RestoreSessionFromCommands(
      request->commands, &(valid_windows.get()));
  static_cast<InternalLastSessionRequest*>(request.get())->
      real_callback->RunWithParams(
          LastSessionCallback::TupleType(request->handle(),
                                         &(valid_windows.get())));
}

void SessionService::RestoreSessionFromCommands(
    const std::vector<SessionCommand*>& commands,
    std::vector<SessionWindow*>* valid_windows) {
  std::map<int, SessionTab*> tabs;
  std::map<int, SessionWindow*> windows;

  if (CreateTabsAndWindows(commands, &tabs, &windows)) {
    AddTabsToWindows(&tabs, &windows);
    SortTabsBasedOnVisualOrderAndPrune(&windows, valid_windows);
    UpdateSelectedTabIndex(valid_windows);
  }
  STLDeleteValues(&tabs);
  // Don't delete conents of windows, that is done by the caller as all
  // valid windows are added to valid_windows.
}

void SessionService::UpdateSelectedTabIndex(
    std::vector<SessionWindow*>* windows) {
  for (std::vector<SessionWindow*>::const_iterator i = windows->begin();
       i != windows->end(); ++i) {
    // See note in SessionWindow as to why we do this.
    int new_index = 0;
    for (std::vector<SessionTab*>::const_iterator j = (*i)->tabs.begin();
         j != (*i)->tabs.end(); ++j) {
      if ((*j)->tab_visual_index == (*i)->selected_tab_index) {
        new_index = static_cast<int>(j - (*i)->tabs.begin());
        break;
      }
    }
    (*i)->selected_tab_index = new_index;
  }
}

SessionWindow* SessionService::GetWindow(
    SessionID::id_type window_id,
    IdToSessionWindow* windows) {
  std::map<int, SessionWindow*>::iterator i = windows->find(window_id);
  if (i == windows->end()) {
    SessionWindow* window = new SessionWindow();
    window->window_id.set_id(window_id);
    (*windows)[window_id] = window;
    return window;
  }
  return i->second;
}

SessionTab* SessionService::GetTab(
    SessionID::id_type tab_id,
    IdToSessionTab* tabs) {
  DCHECK(tabs);
  std::map<int, SessionTab*>::iterator i = tabs->find(tab_id);
  if (i == tabs->end()) {
    SessionTab* tab = new SessionTab();
    tab->tab_id.set_id(tab_id);
    (*tabs)[tab_id] = tab;
    return tab;
  }
  return i->second;
}

std::vector<TabNavigation>::iterator
  SessionService::FindClosestNavigationWithIndex(
    std::vector<TabNavigation>* navigations,
    int index) {
  DCHECK(navigations);
  for (std::vector<TabNavigation>::iterator i = navigations->begin();
       i != navigations->end(); ++i) {
    if (i->index() >= index)
      return i;
  }
  return navigations->end();
}

// Function used in sorting windows. Sorting is done based on window id. As
// window ids increment for each new window, this effectively sorts by creation
// time.
static bool WindowOrderSortFunction(const SessionWindow* w1,
                                    const SessionWindow* w2) {
  return w1->window_id.id() < w2->window_id.id();
}

// Compares the two tabs based on visual index.
static bool TabVisualIndexSortFunction(const SessionTab* t1,
                                       const SessionTab* t2) {
  const int delta = t1->tab_visual_index - t2->tab_visual_index;
  return delta == 0 ? (t1->tab_id.id() < t2->tab_id.id()) : (delta < 0);
}

void SessionService::SortTabsBasedOnVisualOrderAndPrune(
    std::map<int, SessionWindow*>* windows,
    std::vector<SessionWindow*>* valid_windows) {
  std::map<int, SessionWindow*>::iterator i = windows->begin();
  while (i != windows->end()) {
    if (i->second->tabs.empty() || i->second->is_constrained ||
        !should_track_changes_for_browser_type(i->second->type)) {
      delete i->second;
      i = windows->erase(i);
    } else {
      // Valid window; sort the tabs and add it to the list of valid windows.
      std::sort(i->second->tabs.begin(), i->second->tabs.end(),
                &TabVisualIndexSortFunction);
      // Add the window such that older windows appear first.
      if (valid_windows->empty()) {
        valid_windows->push_back(i->second);
      } else {
        valid_windows->insert(
            std::upper_bound(valid_windows->begin(), valid_windows->end(),
                             i->second, &WindowOrderSortFunction),
            i->second);
      }
      ++i;
    }
  }
}

void SessionService::AddTabsToWindows(std::map<int, SessionTab*>* tabs,
                                      std::map<int, SessionWindow*>* windows) {
  std::map<int, SessionTab*>::iterator i = tabs->begin();
  while (i != tabs->end()) {
    SessionTab* tab = i->second;
    if (tab->window_id.id() && !tab->navigations.empty()) {
      SessionWindow* window = GetWindow(tab->window_id.id(), windows);
      window->tabs.push_back(tab);
      i = tabs->erase(i);

      // See note in SessionTab as to why we do this.
      std::vector<TabNavigation>::iterator j =
          FindClosestNavigationWithIndex(&(tab->navigations),
                                         tab->current_navigation_index);
      if (j == tab->navigations.end()) {
        tab->current_navigation_index =
            static_cast<int>(tab->navigations.size() - 1);
      } else {
        tab->current_navigation_index =
            static_cast<int>(j - tab->navigations.begin());
      }
    } else {
      // Never got a set tab index in window, or tabs are empty, nothing
      // to do.
      ++i;
    }
  }
}

bool SessionService::CreateTabsAndWindows(
    const std::vector<SessionCommand*>& data,
    std::map<int, SessionTab*>* tabs,
    std::map<int, SessionWindow*>* windows) {
  // If the file is corrupt (command with wrong size, or unknown command), we
  // still return true and attempt to restore what we we can.

  for (std::vector<SessionCommand*>::const_iterator i = data.begin();
       i != data.end(); ++i) {
    const SessionCommand* command = *i;

    switch (command->id()) {
      case kCommandSetTabWindow: {
        SessionID::id_type payload[2];
        if (!command->GetPayload(payload, sizeof(payload)))
          return true;
        GetTab(payload[1], tabs)->window_id.set_id(payload[0]);
        break;
      }

      case kCommandSetWindowBounds2: {
        WindowBoundsPayload2 payload;
        if (!command->GetPayload(&payload, sizeof(payload)))
          return true;
        GetWindow(payload.window_id, windows)->bounds.SetRect(payload.x,
                                                              payload.y,
                                                              payload.w,
                                                              payload.h);
        GetWindow(payload.window_id, windows)->is_maximized =
            payload.is_maximized;
        break;
      }

      case kCommandSetTabIndexInWindow: {
        TabIndexInWindowPayload payload;
        if (!command->GetPayload(&payload, sizeof(payload)))
          return true;
        GetTab(payload.id, tabs)->tab_visual_index = payload.index;
        break;
      }

      case kCommandTabClosed:
      case kCommandWindowClosed: {
        ClosedPayload payload;
        if (!command->GetPayload(&payload, sizeof(payload)))
          return true;
        if (command->id() == kCommandTabClosed) {
          delete GetTab(payload.id, tabs);
          tabs->erase(payload.id);
        } else {
          delete GetWindow(payload.id, windows);
          windows->erase(payload.id);
        }
        break;
      }

      case kCommandTabNavigationPathPrunedFromBack: {
        TabNavigationPathPrunedFromBackPayload payload;
        if (!command->GetPayload(&payload, sizeof(payload)))
          return true;
        SessionTab* tab = GetTab(payload.id, tabs);
        tab->navigations.erase(
            FindClosestNavigationWithIndex(&(tab->navigations), payload.index),
            tab->navigations.end());
        break;
      }

      case kCommandTabNavigationPathPrunedFromFront: {
        TabNavigationPathPrunedFromFrontPayload payload;
        if (!command->GetPayload(&payload, sizeof(payload)) ||
            payload.index <= 0) {
          return true;
        }
        SessionTab* tab = GetTab(payload.id, tabs);

        // Update the selected navigation index.
        tab->current_navigation_index =
            std::max(-1, tab->current_navigation_index - payload.index);
        
        // And update the index of existing navigations.
        for (std::vector<TabNavigation>::iterator i = tab->navigations.begin();
             i != tab->navigations.end();) {
          i->set_index(i->index() - payload.index);
          if (i->index() < 0)
            i = tab->navigations.erase(i);
          else
            ++i;
        }
        break;
      }

      case kCommandUpdateTabNavigation: {
        TabNavigation navigation;
        SessionID::id_type tab_id;
        if (!RestoreUpdateTabNavigationCommand(*command, &navigation, &tab_id))
          return true;

        SessionTab* tab = GetTab(tab_id, tabs);
        std::vector<TabNavigation>::iterator i =
            FindClosestNavigationWithIndex(&(tab->navigations),
                                           navigation.index());
        if (i != tab->navigations.end() && i->index() == navigation.index())
          *i = navigation;
        else
          tab->navigations.insert(i, navigation);
        break;
      }

      case kCommandSetSelectedNavigationIndex: {
        SelectedNavigationIndexPayload payload;
        if (!command->GetPayload(&payload, sizeof(payload)))
          return true;
        GetTab(payload.id, tabs)->current_navigation_index = payload.index;
        break;
      }

      case kCommandSetSelectedTabInIndex: {
        SelectedTabInIndexPayload payload;
        if (!command->GetPayload(&payload, sizeof(payload)))
          return true;
        GetWindow(payload.id, windows)->selected_tab_index = payload.index;
        break;
      }

      case kCommandSetWindowType: {
        WindowTypePayload payload;
        if (!command->GetPayload(&payload, sizeof(payload)))
          return true;
        GetWindow(payload.id, windows)->is_constrained = false;
        GetWindow(payload.id, windows)->type =
            static_cast<Browser::Type>(payload.index);
        break;
      }

      default:
        return true;
    }
  }
  return true;
}

void SessionService::BuildCommandsForTab(
    const SessionID& window_id,
    NavigationController* controller,
    int index_in_window,
    std::vector<SessionCommand*>* commands,
    IdToRange* tab_to_available_range) {
  DCHECK(controller && commands && window_id.id());
  commands->push_back(
      CreateSetTabWindowCommand(window_id, controller->session_id()));
  const int current_index = controller->GetCurrentEntryIndex();
  const int min_index = std::max(0,
                                 current_index - max_persist_navigation_count);
  const int max_index = std::min(current_index + max_persist_navigation_count,
                                 controller->GetEntryCount());
  const int pending_index = controller->GetPendingEntryIndex();
  if (tab_to_available_range) {
    (*tab_to_available_range)[controller->session_id().id()] =
        std::pair<int, int>(min_index, max_index);
  }
  for (int i = min_index; i < max_index; ++i) {
    const NavigationEntry* entry = (i == pending_index) ?
        controller->GetPendingEntry() : controller->GetEntryAtIndex(i);
    DCHECK(entry);
    if (ShouldTrackEntry(*entry)) {
      commands->push_back(
          CreateUpdateTabNavigationCommand(kCommandUpdateTabNavigation,
                                           controller->session_id().id(),
                                           i,
                                           *entry));
    }
  }
  commands->push_back(
      CreateSetSelectedNavigationIndexCommand(controller->session_id(),
                                              current_index));

  if (index_in_window != -1) {
    commands->push_back(
        CreateSetTabIndexInWindowCommand(controller->session_id(),
                                         index_in_window));
  }
}

void SessionService::BuildCommandsForBrowser(
    Browser* browser,
    std::vector<SessionCommand*>* commands,
    IdToRange* tab_to_available_range,
    std::set<SessionID::id_type>* windows_to_track) {
  DCHECK(browser && commands);
  DCHECK(browser->session_id().id());

  commands->push_back(
      CreateSetWindowBoundsCommand(browser->session_id(),
                                   browser->window()->GetNormalBounds(),
                                   browser->window()->IsMaximized()));

  commands->push_back(CreateSetWindowTypeCommand(
      browser->session_id(), browser->type()));

  bool added_to_windows_to_track = false;
  for (int i = 0; i < browser->tab_count(); ++i) {
    TabContents* tab = browser->GetTabContentsAt(i);
    DCHECK(tab);
    if (tab->profile() == profile()) {
      BuildCommandsForTab(browser->session_id(), tab->controller(), i,
                          commands, tab_to_available_range);
      if (windows_to_track && !added_to_windows_to_track) {
        windows_to_track->insert(browser->session_id().id());
        added_to_windows_to_track = true;
      }
    }
  }
  commands->push_back(
      CreateSetSelectedTabInWindow(browser->session_id(),
                                   browser->selected_index()));
}

void SessionService::BuildCommandsFromBrowsers(
    std::vector<SessionCommand*>* commands,
    IdToRange* tab_to_available_range,
    std::set<SessionID::id_type>* windows_to_track) {
  DCHECK(commands);
  for (BrowserList::const_iterator i = BrowserList::begin();
       i != BrowserList::end(); ++i) {
    // Make sure the browser has tabs and a window. Browsers destructor
    // removes itself from the BrowserList. When a browser is closed the
    // destructor is not necessarily run immediately. This means its possible
    // for us to get a handle to a browser that is about to be removed. If
    // the tab count is 0 or the window is NULL, the browser is about to be
    // deleted, so we ignore it.
    if (should_track_changes_for_browser_type((*i)->type()) &&
        (*i)->tab_count() && (*i)->window()) {
      BuildCommandsForBrowser(*i, commands, tab_to_available_range,
                              windows_to_track);
    }
  }
}

void SessionService::ScheduleReset() {
  set_pending_reset(true);
  STLDeleteElements(&pending_commands());
  tab_to_available_range_.clear();
  windows_tracking_.clear();
  BuildCommandsFromBrowsers(&pending_commands(), &tab_to_available_range_,
                            &windows_tracking_);
  if (!windows_tracking_.empty()) {
    // We're lazily created on startup and won't get an initial batch of
    // SetWindowType messages. Set these here to make sure our state is correct.
    has_open_tabbed_browsers_ = true;
    move_on_new_browser_ = true;
  }
  StartSaveTimer();
}

bool SessionService::ReplacePendingCommand(SessionCommand* command) {
  // We only optimize page navigations, which can happen quite frequently and
  // are expensive. If necessary, other commands could be searched for as
  // well.
  if (command->id() != kCommandUpdateTabNavigation)
    return false;
  void* iterator = NULL;
  scoped_ptr<Pickle> command_pickle(command->PayloadAsPickle());
  SessionID::id_type command_tab_id;
  int command_nav_index;
  if (!command_pickle->ReadInt(&iterator, &command_tab_id) ||
      !command_pickle->ReadInt(&iterator, &command_nav_index)) {
    return false;
  }
  for (std::vector<SessionCommand*>::reverse_iterator i =
       pending_commands().rbegin(); i != pending_commands().rend(); ++i) {
    SessionCommand* existing_command = *i;
    if (existing_command->id() == kCommandUpdateTabNavigation) {
      SessionID::id_type existing_tab_id;
      int existing_nav_index;
      scoped_ptr<Pickle> existing_pickle(existing_command->PayloadAsPickle());
      iterator = NULL;
      if (!existing_pickle->ReadInt(&iterator, &existing_tab_id) ||
          !existing_pickle->ReadInt(&iterator, &existing_nav_index)) {
        return false;
      }
      if (existing_tab_id == command_tab_id &&
          existing_nav_index == command_nav_index) {
        // existing_command is an update for the same tab/index pair. Replace
        // it with the new one. We need to add to the end of the list just in
        // case there is a prune command after the update command.
        delete existing_command;
        pending_commands().erase(i.base() - 1);
        pending_commands().push_back(command);
        return true;
      }
      return false;
    }
  }
  return false;
}

void SessionService::ScheduleCommand(SessionCommand* command) {
  DCHECK(command);
  if (ReplacePendingCommand(command))
    return;
  BaseSessionService::ScheduleCommand(command);
  // Don't schedule a reset on tab closed/window closed. Otherwise we may
  // lose tabs/windows we want to restore from if we exit right after this.
  if (!pending_reset() && pending_window_close_ids_.empty() &&
      commands_since_reset() >= kWritesPerReset &&
      (command->id() != kCommandTabClosed &&
       command->id() != kCommandWindowClosed)) {
    ScheduleReset();
  }
}

void SessionService::CommitPendingCloses() {
  for (PendingTabCloseIDs::iterator i = pending_tab_close_ids_.begin();
       i != pending_tab_close_ids_.end(); ++i) {
    ScheduleCommand(CreateTabClosedCommand(*i));
  }
  pending_tab_close_ids_.clear();

  for (PendingWindowCloseIDs::iterator i = pending_window_close_ids_.begin();
       i != pending_window_close_ids_.end(); ++i) {
    ScheduleCommand(CreateWindowClosedCommand(*i));
  }
  pending_window_close_ids_.clear();
}

bool SessionService::IsOnlyOneTabLeft() {
  if (!profile()) {
    // We're testing, always return false.
    return false;
  }

  // NOTE: This uses the original profile so that closing the last non-off the
  // record window while there are open off the record window resets state).
  int window_count = 0;
  for (BrowserList::const_iterator i = BrowserList::begin();
       i != BrowserList::end(); ++i) {
    const SessionID::id_type window_id = (*i)->session_id().id();
    if (should_track_changes_for_browser_type((*i)->type()) &&
        (*i)->profile()->GetOriginalProfile() == profile() &&
        window_closing_ids_.find(window_id) == window_closing_ids_.end()) {
      if (++window_count > 1)
        return false;
      // By the time this is invoked the tab has been removed. As such, we use
      // > 0 here rather than > 1.
      if ((*i)->tab_count() > 0)
        return false;
    }
  }
  return true;
}

bool SessionService::HasOpenTabbedBrowsers(const SessionID& window_id) {
  if (!profile()) {
    // We're testing, always return false.
    return true;
  }

  // NOTE: This uses the original profile so that closing the last non-off the
  // record window while there are open off the record window resets state).
  for (BrowserList::const_iterator i = BrowserList::begin();
       i != BrowserList::end(); ++i) {
    Browser* browser = *i;
    const SessionID::id_type browser_id = browser->session_id().id();
    if (browser_id != window_id.id() &&
        window_closing_ids_.find(browser_id) == window_closing_ids_.end() &&
        should_track_changes_for_browser_type(browser->type()) &&
        browser->profile()->GetOriginalProfile() == profile()) {
      return true;
    }
  }
  return false;
}

bool SessionService::ShouldTrackChangesToWindow(const SessionID& window_id) {
  return windows_tracking_.find(window_id.id()) != windows_tracking_.end();
}
