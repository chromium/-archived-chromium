// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/session_service.h"

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
#include "chrome/browser/session_backend.h"
#include "chrome/browser/session_restore.h"
#include "chrome/browser/session_startup_pref.h"
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

// Max number of navigation entries in each direction we'll persist.
static const int kMaxNavigationCountToPersist = 6;

// Delay between when a command is received, and when we save it to the
// backend.
static const int kSaveDelayMS = 2500;

// Every kWritesPerReset commands triggers recreating the file.
static const int kWritesPerReset = 250;

namespace {

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

// Helper used by CreateUpdateTabNavigationCommand(). It writes |str| to
// |pickle|, if and only if |str| fits within (|max_bytes| - |*bytes_written|).
// |bytes_written| is incremented to reflect the data written.
void WriteStringToPickle(Pickle& pickle, int* bytes_written, int max_bytes,
                         const std::string& str) {
  int num_bytes = str.size() * sizeof(char);
  if (*bytes_written + num_bytes < max_bytes) {
    *bytes_written += num_bytes;
    pickle.WriteString(str);
  } else {
    pickle.WriteString(std::string());
  }
}

// Wide version of WriteStringToPickle.
void WriteWStringToPickle(Pickle& pickle, int* bytes_written, int max_bytes,
                          const std::wstring& str) {
  int num_bytes = str.size() * sizeof(wchar_t);
  if (*bytes_written + num_bytes < max_bytes) {
    *bytes_written += num_bytes;
    pickle.WriteWString(str);
  } else {
    pickle.WriteWString(std::wstring());
  }
}

}  // namespace

// SessionID ------------------------------------------------------------------

static SessionID::id_type next_id = 1;

SessionID::SessionID() {
  id_ = next_id++;
}

// SessionService -------------------------------------------------------------

SessionService::SessionService(Profile* profile)
    : profile_(profile),
#pragma warning(suppress: 4355)  // Okay to pass "this" here.
      save_factory_(this),
      pending_reset_(false),
      has_open_tabbed_browsers_(false),
      move_on_new_browser_(false) {
  DCHECK(profile);
  // We should never be created when off the record.
  DCHECK(!profile->IsOffTheRecord());
  Init(profile->GetPath());
}

SessionService::SessionService(const std::wstring& save_path)
    : profile_(NULL),
#pragma warning(suppress: 4355)  // Okay to pass "this" here.
      save_factory_(this),
      pending_reset_(false),
      has_open_tabbed_browsers_(false),
      move_on_new_browser_(false) {
  Init(save_path);
}

SessionService::~SessionService() {
  if (!backend_.get())
    return;
  Save();
  // If no pending requests, then the backend closes immediately and is
  // deleted. Otherwise the backend is deleted after all pending requests on
  // the file thread complete, which is done before the process exits.
  backend_ = NULL;

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

  if (!backend_thread_) {
    backend_->MoveCurrentSessionToLastSession();
  } else {
    backend_thread_->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
        backend_.get(), &SessionBackend::MoveCurrentSessionToLastSession));
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
  ScheduleCommand(CreateUpdateTabNavigationCommand(tab_id, index, entry));
}

void SessionService::TabRestored(NavigationController* controller) {
  if (!ShouldTrackChangesToWindow(controller->window_id()))
    return;

  BuildCommandsForTab(controller->window_id(), controller, -1,
                      &pending_commands_, NULL);
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

SessionService::Handle SessionService::GetSavedSession(
    CancelableRequestConsumerBase* consumer,
    SavedSessionCallback* callback) {
  return GetSessionImpl(consumer, callback, true);
}

SessionService::Handle SessionService::GetLastSession(
    CancelableRequestConsumerBase* consumer,
    SavedSessionCallback* callback) {
  return GetSessionImpl(consumer, callback, false);
}

void SessionService::CreateSavedSession() {
  std::vector<SessionCommand*> commands;
  // Commands are freed by backend.
  BuildCommandsFromBrowsers(&commands, NULL, NULL);
  if (!backend_thread_) {
    backend_->SaveSession(commands);
  } else {
    backend_thread_->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
        backend_.get(), &SessionBackend::SaveSession, commands));
  }
}

void SessionService::DeleteSession(bool saved_session) {
  if (!backend_thread_) {
    backend_->DeleteSession(saved_session);
  } else {
    backend_thread_->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
        backend_.get(), &SessionBackend::DeleteSession, saved_session));
  }
}

void SessionService::CopyLastSessionToSavedSession() {
  if (!backend_thread_) {
    backend_->CopyLastSessionToSavedSession();
  } else {
    backend_thread_->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
        backend_.get(), &SessionBackend::CopyLastSessionToSavedSession));
  }
}

void SessionService::Init(const std::wstring& path) {
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

  DCHECK(!path.empty());
  commands_since_reset_ = 0;
  backend_ = new SessionBackend(path);
  backend_thread_ = g_browser_process->file_thread();
  if (!backend_thread_)
    backend_->Init();
  // If backend_thread, backend will init itself as appropriate.
}

void SessionService::Observe(NotificationType type,
                             const NotificationSource& source,
                             const NotificationDetails& details) {
  // All of our messages have the NavigationController as the source.
  switch (type) {
    case NOTIFY_BROWSER_OPENED: {
      Browser* browser = Source<Browser>(source).ptr();
      if (browser->profile() != profile_ ||
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
        SessionStartupPref pref = SessionStartupPref::GetStartupPref(profile_);
        if (pref.type == SessionStartupPref::LAST) {
          SessionRestore::RestoreSession(
              profile_, browser, false, false, false, std::vector<GURL>());
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

SessionService::Handle SessionService::GetSessionImpl(
    CancelableRequestConsumerBase* consumer,
    SavedSessionCallback* callback,
    bool is_saved_session) {
  scoped_refptr<InternalSavedSessionRequest> request(
      new InternalSavedSessionRequest(
          NewCallback(this, &SessionService::OnGotSessionCommands),
          callback,
          is_saved_session));
  AddRequest(request.get(), consumer);
  backend_thread_->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
      backend_.get(), &SessionBackend::ReadSession, request));
  return request->handle();
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

SessionCommand* SessionService::CreateUpdateTabNavigationCommand(
    const SessionID& tab_id,
    int index,
    const NavigationEntry& entry) {
  // Use pickle to handle marshalling.
  Pickle pickle;
  pickle.WriteInt(tab_id.id());
  pickle.WriteInt(index);

  // We only allow navigations up to 63k (which should be completely
  // reasonable). On the off chance we get one that is too big, try to
  // keep the url.

  // Bound the string data (which is variable length) to
  // |max_state_size bytes| bytes.
  static const SessionCommand::size_type max_state_size =
      std::numeric_limits<SessionCommand::size_type>::max() - 1024;

  int bytes_written = 0;
  
  WriteStringToPickle(pickle, &bytes_written, max_state_size,
                      entry.display_url().spec());

  WriteWStringToPickle(pickle, &bytes_written, max_state_size,
                       entry.title());

  WriteStringToPickle(pickle, &bytes_written, max_state_size,
                      entry.content_state());

  pickle.WriteInt(entry.transition_type());
  int type_mask = entry.has_post_data() ? TabNavigation::HAS_POST_DATA : 0;
  pickle.WriteInt(type_mask);

  WriteStringToPickle(pickle, &bytes_written, max_state_size,
      entry.referrer().is_valid() ? entry.referrer().spec() : std::string());

  // Adding more data? Be sure and update TabRestoreService too.
  return new SessionCommand(kCommandUpdateTabNavigation, pickle);
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

void SessionService::OnGotSessionCommands(
    Handle handle,
    scoped_refptr<InternalSavedSessionRequest> request) {
  if (request->canceled())
    return;
  ScopedVector<SessionWindow> valid_windows;
  RestoreSessionFromCommands(
      request->commands, &(valid_windows.get()));
  request->real_callback->RunWithParams(
      SavedSessionCallback::TupleType(request->handle(),
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
    if (i->index >= index)
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
          i->index -= payload.index;
          if (i->index < 0)
            i = tab->navigations.erase(i);
          else
            ++i;
        }
        break;
      }

      case kCommandUpdateTabNavigation: {
        scoped_ptr<Pickle> pickle(command->PayloadAsPickle());
        if (!pickle.get())
          return true;
        TabNavigation navigation;
        SessionID::id_type tab_id;
        void* iterator = NULL;
        std::string url_spec;
        if (!pickle->ReadInt(&iterator, &tab_id) ||
            !pickle->ReadInt(&iterator, &(navigation.index)) ||
            !pickle->ReadString(&iterator, &url_spec) ||
            !pickle->ReadWString(&iterator, &(navigation.title)) ||
            !pickle->ReadString(&iterator, &(navigation.state)) ||
            !pickle->ReadInt(&iterator,
                             reinterpret_cast<int*>(&(navigation.transition))))
          return true;
        // type_mask did not always exist in the written stream. As such, we
        // don't fail if it can't be read.
        bool has_type_mask =
            pickle->ReadInt(&iterator, &(navigation.type_mask));

        if (has_type_mask) {
          // the "referrer" property was added after type_mask to the written
          // stream. As such, we don't fail if it can't be read.
          std::string referrer_spec;
          pickle->ReadString(&iterator, &referrer_spec);
          if (!referrer_spec.empty()) {
            navigation.referrer = GURL(referrer_spec);
          }
        }

        navigation.url = GURL(url_spec);
        SessionTab* tab = GetTab(tab_id, tabs);
        std::vector<TabNavigation>::iterator i =
            FindClosestNavigationWithIndex(&(tab->navigations),
                                           navigation.index);
        if (i != tab->navigations.end() && i->index == navigation.index)
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
                                 current_index - kMaxNavigationCountToPersist);
  const int max_index = std::min(current_index + kMaxNavigationCountToPersist,
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
          CreateUpdateTabNavigationCommand(controller->session_id(),
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
    if (tab->profile() == profile_) {
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
    if (should_track_changes_for_browser_type((*i)->type())) {
      BuildCommandsForBrowser(*i, commands, tab_to_available_range,
                              windows_to_track);
    }
  }
}

void SessionService::ScheduleReset() {
  pending_reset_ = true;
  STLDeleteElements(&pending_commands_);
  tab_to_available_range_.clear();
  windows_tracking_.clear();
  BuildCommandsFromBrowsers(&pending_commands_, &tab_to_available_range_,
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
       pending_commands_.rbegin(); i != pending_commands_.rend(); ++i) {
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
        pending_commands_.erase(i.base() - 1);
        pending_commands_.push_back(command);
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
  commands_since_reset_++;
  pending_commands_.push_back(command);
  // Don't schedule a reset on tab closed/window closed. Otherwise we may
  // lose tabs/windows we want to restore from if we exit right after this.
  if (!pending_reset_ && pending_window_close_ids_.empty() &&
      commands_since_reset_ >= kWritesPerReset &&
      (command->id() != kCommandTabClosed &&
       command->id() != kCommandWindowClosed)) {
    ScheduleReset();
  }
  StartSaveTimer();
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

void SessionService::Save() {
  DCHECK(backend_.get());

  if (pending_commands_.empty())
    return;

  if (!backend_thread_) {
    backend_->AppendCommands(
        new std::vector<SessionCommand*>(pending_commands_), pending_reset_);
  } else {
    backend_thread_->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
        backend_.get(), &SessionBackend::AppendCommands,
        new std::vector<SessionCommand*>(pending_commands_),
        pending_reset_));
  }
  if (pending_reset_) {
    commands_since_reset_ = 0;
    pending_reset_ = false;
  }
  // Backend took ownership of commands.
  pending_commands_.clear();
}

void SessionService::StartSaveTimer() {
  // Don't start a timer when testing (profile == NULL).
  if (profile_ && save_factory_.empty()) {
    MessageLoop::current()->PostDelayedTask(FROM_HERE,
        save_factory_.NewRunnableMethod(&SessionService::Save), kSaveDelayMS);
  }
}

bool SessionService::IsOnlyOneTabLeft() {
  if (!profile_) {
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
        (*i)->profile()->GetOriginalProfile() == profile_ &&
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
  if (!profile_) {
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
        browser->profile()->GetOriginalProfile() == profile_) {
      return true;
    }
  }
  return false;
}

bool SessionService::ShouldTrackChangesToWindow(const SessionID& window_id) {
  return windows_tracking_.find(window_id.id()) != windows_tracking_.end();
}

bool SessionService::ShouldTrackEntry(const NavigationEntry& entry) {
  // Don't track entries that have post data. Post data may contain passwords
  // and other sensitive data users don't want stored to disk.
  return entry.display_url().is_valid() && !entry.has_post_data();
}

// InternalSavedSessionRequest ------------------------------------------------

SessionService::InternalSavedSessionRequest::~InternalSavedSessionRequest() {
  STLDeleteElements(&commands);
}
