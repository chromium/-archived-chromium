// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_BASE_SESSION_SERVICE_H_
#define CHROME_BROWSER_SESSIONS_BASE_SESSION_SERVICE_H_

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/ref_counted.h"
#include "base/task.h"
#include "chrome/browser/cancelable_request.h"
#include "chrome/browser/sessions/session_id.h"

class NavigationEntry;
class Profile;
class SessionBackend;
class SessionCommand;
class SessionService;
class TabNavigation;

namespace base {
class Thread;
}

// BaseSessionService is the super class of both tab restore service and
// session service. It contains commonality needed by both, in particular
// it manages a set of SessionCommands that are periodically sent to a
// SessionBackend.
class BaseSessionService : public CancelableRequestProvider,
    public base::RefCountedThreadSafe<BaseSessionService> {
 public:
  // Identifies the type of session service this is. This is used by the
  // backend to determine the name of the files.
  enum SessionType {
    SESSION_RESTORE,
    TAB_RESTORE
  };

  // Creates a new BaseSessionService. After creation you need to invoke
  // Init.
  // |type| gives the type of session service, |profile| the profile and
  // |path| the path to save files to. If |profile| is non-NULL, |path| is
  // ignored and instead the path comes from the profile.
  BaseSessionService(SessionType type,
                     Profile* profile,
                     const FilePath& path);

  virtual ~BaseSessionService();

  Profile* profile() const { return profile_; }

  // Deletes the last session.
  void DeleteLastSession();

  class InternalGetCommandsRequest;

  typedef Callback2<Handle, scoped_refptr<InternalGetCommandsRequest> >::Type
      InternalGetCommandsCallback;

  // Callback used when fetching the last session. The last session consists
  // of a vector of SessionCommands.
  class InternalGetCommandsRequest :
      public CancelableRequest<InternalGetCommandsCallback> {
   public:
    explicit InternalGetCommandsRequest(CallbackType* callback)
        : CancelableRequest(callback) {
    }
    virtual ~InternalGetCommandsRequest();

    // The commands. The backend fills this in for us.
    std::vector<SessionCommand*> commands;

   private:
    DISALLOW_COPY_AND_ASSIGN(InternalGetCommandsRequest);
  };

 protected:
  // Returns the backend.
  SessionBackend* backend() const { return backend_; }

  // Returns the thread the backend runs on. This returns NULL during testing.
  base::Thread* backend_thread() const { return backend_thread_; }

  // Returns the set of commands that needed to be scheduled. The commands
  // in the vector are owned by BaseSessionService, until they are scheduled
  // on the backend at which point the backend owns the commands.
  std::vector<SessionCommand*>&  pending_commands() {
    return pending_commands_;
  }

  // Whether the next save resets the file before writing to it.
  void set_pending_reset(bool value) { pending_reset_ = value; }
  bool pending_reset() const { return pending_reset_; }

  // Returns the number of commands sent down since the last reset.
  int commands_since_reset() const { return commands_since_reset_; }

  // Schedules a command. This adds |command| to pending_commands_ and
  // invokes StartSaveTimer to start a timer that invokes Save at a later
  // time.
  virtual void ScheduleCommand(SessionCommand* command);

  // Starts the timer that invokes Save (if timer isn't already running).
  void StartSaveTimer();

  // Saves pending commands to the backend. This is invoked from the timer
  // scheduled by StartSaveTimer.
  virtual void Save();

  // Creates a SessionCommand that represents a navigation.
  SessionCommand* CreateUpdateTabNavigationCommand(
      SessionID::id_type command_id,
      SessionID::id_type tab_id,
      int index,
      const NavigationEntry& entry);

  // Converts a SessionCommand previously created by
  // CreateUpdateTabNavigationCommand into a TabNavigation. Returns true
  // on success. If successful |tab_id| is set to the id of the restored tab.
  bool RestoreUpdateTabNavigationCommand(const SessionCommand& command,
                                         TabNavigation* navigation,
                                         SessionID::id_type* tab_id);

  // Returns true if the NavigationEntry should be written to disk.
  bool ShouldTrackEntry(const NavigationEntry& entry);

  // Returns true if the TabNavigationshould be written to disk.
  bool ShouldTrackEntry(const TabNavigation& navigation);

  // Invokes ReadLastSessionCommands with request on the backend thread.
  // If testing, ReadLastSessionCommands is invoked directly.
  Handle ScheduleGetLastSessionCommands(
      InternalGetCommandsRequest* request,
      CancelableRequestConsumerBase* consumer);

  // Max number of navigation entries in each direction we'll persist.
  static const int max_persist_navigation_count;

 private:
  // The profile. This may be null during testing.
  Profile* profile_;

  // Path to read from. This is only used if profile_ is NULL.
  const FilePath& path_;

  // The backend.
  scoped_refptr<SessionBackend> backend_;

  // Thread backend tasks are run on, is NULL during testing.
  base::Thread* backend_thread_;

  // Used to invoke Save.
  ScopedRunnableMethodFactory<BaseSessionService> save_factory_;

  // Commands we need to send over to the backend.
  std::vector<SessionCommand*>  pending_commands_;

  // Whether the backend file should be recreated the next time we send
  // over the commands.
  bool pending_reset_;

  // The number of commands sent to the backend before doing a reset.
  int commands_since_reset_;

  DISALLOW_COPY_AND_ASSIGN(BaseSessionService);
};

#endif  // CHROME_BROWSER_SESSIONS_BASE_SESSION_SERVICE_H_
