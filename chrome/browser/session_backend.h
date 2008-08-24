// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSION_BACKEND_H__
#define CHROME_BROWSER_SESSION_BACKEND_H__

#include <vector>

#include "base/ref_counted.h"
#include "base/scoped_handle.h"
#include "chrome/browser/session_service.h"

class Pickle;

// SessionCommand -------------------------------------------------------------

// SessionCommand contains a command id and arbitrary amount chunk of memory.
//
// SessionBackend reads and writes SessionCommands.
//
// A SessionCommand may be created directly from a Pickle, which is useful
// for types of arbitrary length.
class SessionCommand {
 public:
  // These get written to disk, so we define types for them.
  // Type for the identifier.
  typedef uint8 id_type;
  // Type for writing the size.
  typedef uint16 size_type;

  // Creates a session command with the specified id. This allocates a buffer
  // of size |size| that must be filled via contents().
  SessionCommand(id_type id, size_type size);

  // Convenience constructor that creates a session command with the specified
  // id whose contents is populated from the contents of pickle.
  SessionCommand(id_type id, const Pickle& pickle);

  // The contents of the command.
  char* contents() { return &(contents_[0]); }
  const char* contents() const { return &(contents_[0]); }

  // Identifier for the command.
  id_type id() const { return id_; }

  // Size of data.
  size_type size() const { return static_cast<size_type>(contents_.size()); }

  // Convenience for extracting the data to a target. Returns false if
  // count is not equal to the size of data this command contains.
  bool GetPayload(void* dest, size_t count) const;

  // Returns the contents as a pickle. It is up to the caller to delete the
  // returned Pickle. The returned Pickle references the underlying data of
  // this SessionCommand. If you need it to outlive the command, copy the
  // pickle.
  Pickle* PayloadAsPickle() const;

 private:
  const id_type id_;
  std::string contents_;

  DISALLOW_EVIL_CONSTRUCTORS(SessionCommand);
};

// SessionBackend -------------------------------------------------------------

// SessionBackend is the backend used by SessionService. It is responsible
// for maintaining up to 3 files:
// . The current file, which is the file commands passed to AppendCommands
//   get written to.
// . The last file. When created the current file is moved to the last
//   file.
// . A save file, which is created with arbitrary commands.
//
// Each file contains an arbitrary set of commands supplied from
// SessionService.

class SessionBackend : public base::RefCountedThreadSafe<SessionBackend> {
 public:
  typedef SessionCommand::id_type id_type;
  typedef SessionCommand::size_type size_type;

  // Initial size of the buffer used in reading the file. This is exposed
  // for testing.
  static const int kFileReadBufferSize;

  // Creates a SessionBackend. This method is invoked on the MAIN thread,
  // and does NO IO. The real work is done from Init, which is invoked on
  // the file thread.
  //
  // The supplied path is the directory the files are writen to.
  explicit SessionBackend(const std::wstring& path_to_dir);

  // Moves the current file to the last file, and recreates the current file.
  //
  // NOTE: this is invoked before every command, and does nothing if we've
  // already Init'ed.
  void Init();

  // Recreates the save file with the specified commands.
  //
  // This deletes the SessionCommands passed to it.
  void SaveSession(const std::vector<SessionCommand*>& commands);

  // Appends the specified commands to the current file. If reset_first is
  // true the the current file is recreated.
  //
  // NOTE: this deletes SessionCommands in commands as well as the supplied
  // vector.
  void AppendCommands(std::vector<SessionCommand*>* commands,
                      bool reset_first);

  // Invoked from the service, invokes ReadSessionImpl to do the work.
  void ReadSession(
      scoped_refptr<SessionService::InternalSavedSessionRequest> request);

  // Reads the commands from the last file, or save file if
  // use_save_file is true.
  //
  // On success, the read commands are added to commands. It is up to the
  // caller to delete the commands.
  bool ReadSessionImpl(bool use_save_file,
                       std::vector<SessionCommand*>* commands);

  // If saved_session is true, deletes the saved session, otherwise deletes
  // the last file.
  void DeleteSession(bool saved_session);

  // Copies the contents of the last session file to the saved session file.
  void CopyLastSessionToSavedSession();

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
  HANDLE OpenAndWriteHeader(const std::wstring& path);

  // Appends the specified commands to the specified file.
  bool AppendCommandsToFile(HANDLE handle,
                            const std::vector<SessionCommand*>& commands);

  // Returns the path to the last file.
  std::wstring GetLastSessionPath();

  // Returns the path to the save file.
  std::wstring GetSavedSessionPath();

  // Returns the path to the current file.
  std::wstring GetCurrentSessionPath();

  // Directory files are relative to.
  const std::wstring path_to_dir_;

  // Whether the previous target file is valid.
  bool last_session_valid_;

  // Handle to the target file.
  ScopedHandle current_session_handle_;

  // Whether we've inited. Remember, the constructor is run on the
  // Main thread, all others on the IO thread, hence lazy initialization.
  bool inited_;

  // If true, the file is empty (no commands have been added to it).
  bool empty_file_;

  DISALLOW_EVIL_CONSTRUCTORS(SessionBackend);
};

#endif  // #define CHROME_BROWSER_SESSION_BACKEND_H__

