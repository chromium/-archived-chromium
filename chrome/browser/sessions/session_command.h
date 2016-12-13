// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_SESSION_COMMAND_H_
#define CHROME_BROWSER_SESSIONS_SESSION_COMMAND_H_

#include <string>

#include "base/basictypes.h"

class Pickle;

// SessionCommand contains a command id and arbitrary chunk of data. The id
// and chunk of data are specific to the service creating them.
//
// Both TabRestoreService and SessionService use SessionCommands to represent
// state on disk.
//
// There are two ways to create a SessionCommand:
// . Specificy the size of the data block to create. This is useful for
//   commands that have a fixed size.
// . From a pickle, this is useful for commands whose length varies.
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

  DISALLOW_COPY_AND_ASSIGN(SessionCommand);
};

#endif  // CHROME_BROWSER_SESSIONS_SESSION_COMMAND_H_
