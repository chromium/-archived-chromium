// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_SESSION_BACKEND_H_
#define CHROME_BROWSER_SESSIONS_SESSION_BACKEND_H_

#include <vector>

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/sessions/session_command.h"
#include "net/base/file_stream.h"

// TODO(port): Get rid of this section and finish porting.
#if defined(OS_WIN)
#include "chrome/browser/sessions/base_session_service.h"
#else
#include "chrome/common/temp_scaffolding_stubs.h"
#endif

class Pickle;

// SessionBackend -------------------------------------------------------------

// SessionBackend is the backend used by BaseSessionService. It is responsible
// for maintaining two files:
// . The current file, which is the file commands passed to AppendCommands
//   get written to.
// . The last file. When created the current file is moved to the last
//   file.
//
// Each file contains an arbitrary set of commands supplied from
// BaseSessionService. A command consists of a unique id and a stream of bytes.
// SessionBackend does not use the id in anyway, that is used by
// BaseSessionService.
class SessionBackend : public base::RefCountedThreadSafe<SessionBackend> {
 public:
  typedef SessionCommand::id_type id_type;
  typedef SessionCommand::size_type size_type;

  // Initial size of the buffer used in reading the file. This is exposed
  // for testing.
  static const int kFileReadBufferSize;

  // Creates a SessionBackend. This method is invoked on the MAIN thread,
  // and does no IO. The real work is done from Init, which is invoked on
  // the file thread.
  //
  // |path_to_dir| gives the path the files are written two, and |type|
  // indicates which service is using this backend. |type| is used to determine
  // the name of the files to use as well as for logging.
  SessionBackend(BaseSessionService::SessionType type,
                 const FilePath& path_to_dir);

  // Moves the current file to the last file, and recreates the current file.
  //
  // NOTE: this is invoked before every command, and does nothing if we've
  // already Init'ed.
  void Init();

  // Appends the specified commands to the current file. If reset_first is
  // true the the current file is recreated.
  //
  // NOTE: this deletes SessionCommands in commands as well as the supplied
  // vector.
  void AppendCommands(std::vector<SessionCommand*>* commands,
                      bool reset_first);

  // Invoked from the service to read the commands that make up the last
  // session, invokes ReadSessionImpl to do the work.
  void ReadLastSessionCommands(
      scoped_refptr<BaseSessionService::InternalGetCommandsRequest> request);

  // Reads the commands from the last file.
  //
  // On success, the read commands are added to commands. It is up to the
  // caller to delete the commands.
  bool ReadLastSessionCommandsImpl(std::vector<SessionCommand*>* commands);

  // Deletes the file containing the commands for the last session.
  void DeleteLastSession();

  // Moves the current session to the last and resets the current. This is
  // called during startup and if the user launchs the app and no tabbed
  // browsers are running.
  void MoveCurrentSessionToLastSession();

 private:
  // Recreates the current file such that it only contains the header and
  // NO commands.
  void ResetFile();

  // Opens the current file and writes the header. On success a handle to
  // the file is returned.
  net::FileStream* OpenAndWriteHeader(const FilePath& path);

  // Appends the specified commands to the specified file.
  bool AppendCommandsToFile(net::FileStream* file,
                            const std::vector<SessionCommand*>& commands);

  const BaseSessionService::SessionType type_;

  // Returns the path to the last file.
  FilePath GetLastSessionPath();

  // Returns the path to the current file.
  FilePath GetCurrentSessionPath();

  // Directory files are relative to.
  const FilePath path_to_dir_;

  // Whether the previous target file is valid.
  bool last_session_valid_;

  // Handle to the target file.
  scoped_ptr<net::FileStream> current_session_file_;

  // Whether we've inited. Remember, the constructor is run on the
  // Main thread, all others on the IO thread, hence lazy initialization.
  bool inited_;

  // If true, the file is empty (no commands have been added to it).
  bool empty_file_;

  DISALLOW_COPY_AND_ASSIGN(SessionBackend);
};

#endif  // #define CHROME_BROWSER_SESSIONS_SESSION_BACKEND_H_
