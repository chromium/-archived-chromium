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

#ifndef CHROME_COMMON_LOGGING_CHROME_H__
#define CHROME_COMMON_LOGGING_CHROME_H__

#include <string>
#include <vector>

#include "base/logging.h"

class CommandLine;

namespace logging {

// Call to initialize logging for Chrome. This sets up the chrome-specific
// logfile naming scheme and might do other things like log modules and
// setting levels in the future.
//
// The main process might want to delete any old log files on startup by
// setting delete_old_log_file, but the renderer processes should not, or
// they will delete each others' logs.
//
// XXX
// Setting suppress_error_dialogs to true disables any dialogs that would
// normally appear for assertions and crashes, and makes any catchable
// errors (namely assertions) available via GetSilencedErrorCount()
// and GetSilencedError().
void InitChromeLogging(const CommandLine& command_line,
                       OldFileDeletionState delete_old_log_file);

// Call when done using logging for Chrome.
void CleanupChromeLogging();

// Returns the fully-qualified name of the log file.
std::wstring GetLogFileName();

// Returns true when error/assertion dialogs are to be shown,
// false otherwise.
bool DialogsAreSuppressed();

typedef std::vector<std::wstring> AssertionList;

// Gets the list of fatal assertions in the current log file, and
// returns the number of fatal assertions.  (If you don't care
// about the actual list of assertions, you can pass in NULL.)
// NOTE: Since this reads the log file to determine the assertions,
// this operation is O(n) over the length of the log.
// NOTE: This can fail if the file is locked for writing.  However,
// this is unlikely as this function is most useful after
// the program writing the log has terminated.
size_t GetFatalAssertions(AssertionList* assertions);

} // namespace logging

#endif
