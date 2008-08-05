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

#ifndef CHROME_APP_RESULT_CODES_H__
#define CHROME_APP_RESULT_CODES_H__

// This file consolidates all the return codes for the browser and renderer
// process. The return code is the value that:
// a) is returned by main() or winmain(), or
// b) specified in the call for ExitProcess() or TerminateProcess(), or
// c) the exception value that causes a process to terminate.
//
// It is advisable to not use negative numbers because the Windows API returns
// it as an unsigned long and the exception values have high numbers. For
// example EXCEPTION_ACCESS_VIOLATION value is 0xC0000005.

class ResultCodes {
 public:
  enum ExitCode {
    NORMAL_EXIT = 0,            // Normal termination. Keep it *always* zero.
    INVALID_CMDLINE_URL,        // An invalid command line url was given.
    SBOX_INIT_FAILED,           // The sandbox could not be initialized.
    GOOGLE_UPDATE_INIT_FAILED,  // The Google Update client stub init failed.
    GOOGLE_UPDATE_LAUNCH_FAILED,// Google Update could not launch chrome DLL.
    BAD_PROCESS_TYPE,           // The process is of an unknown type.
    MISSING_PATH,               // An critical chrome path is missing.
    MISSING_DATA,               // A critical chrome file is missing.
    SHELL_INTEGRATION_FAILED,   // Failed to make Chrome default browser.
    UNINSTALL_DELETE_FILE_ERROR,// Error while deleting shortcuts.
    UNINSTALL_CHROME_ALIVE,     // Uninstall detected another chrome instance.
    UNINSTALL_NO_SURVEY,        // Do not launch survey after uninstall.
    UNINSTALL_USER_CANCEL,      // The user changed her mind.
    UNSUPPORTED_PARAM,          // Command line parameter is not supported.
    KILLED_BAD_MESSAGE,         // A bad message caused the process termination.
    IMPORTER_CANCEL,            // The user canceled the browser import.
    HUNG,                       // Browser was hung and killed.
    IMPORTER_HUNG,              // Browser import hung and was killed.
    EXIT_LAST_CODE              // Last return code (keep it last).
  };
};

#endif  // CHROME_APP_RESULT_CODES_H__
