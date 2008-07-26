// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef CHROME_BROWSER_SESSION_RESTORE_H__
#define CHROME_BROWSER_SESSION_RESTORE_H__

#include <vector>

#include "chrome/browser/history/history.h"
#include "base/basictypes.h"

class Browser;
class Profile;

// SessionRestore handles restoring either the last or saved session. Session
// restore come in two variants, asynchronous or synchronous. The synchronous
// variety is meant for startup, and blocks until restore is complete.
class SessionRestore {
 public:
  // Asnchronously restores the specified session.
  // If clobber_existing_window is true and there is an open browser window,
  // it is closed after restoring.
  // If always_create_tabbed_browser is true at least one tabbed browser is
  // created. For example, if there is an error restoring, or the last session
  // session is empty and always_create_tabbed_browser is true, a new empty
  // tabbed browser is created.
  //
  // If urls_to_open is non-empty, a tab is added for each of the URLs.
  static void RestoreSession(Profile* profile,
                             bool use_saved_session,
                             bool clobber_existing_window,
                             bool always_create_tabbed_browser,
                             const std::vector<GURL>& urls_to_open);

  // Synchronously restores the last session. At least one tabbed browser is
  // created, even if there is an error in restoring.
  //
  // If urls_to_open is non-empty, a tab is added for each of the URLs.
  static void RestoreSessionSynchronously(
      Profile* profile,
      bool use_saved_session,
      int show_command,
      const std::vector<GURL>& urls_to_open);

  // The max number of non-selected tabs SessionRestore loads when restoring
  // a session. A value of 0 indicates all tabs are loaded at once.
  static size_t num_tabs_to_load_;

 private:
  SessionRestore();

  DISALLOW_EVIL_CONSTRUCTORS(SessionRestore);
};

#endif  // CHROME_BROWSER_SESSION_RESTORE_H__
