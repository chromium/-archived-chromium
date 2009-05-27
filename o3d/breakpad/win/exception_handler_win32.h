/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

//
// Wrapper class for using the Breakpad crash reporting system.
// (adapted from code in Google Gears)
//

#ifndef O3D_BREAKPAD_WIN_EXCEPTION_HANDLER_WIN32_H_
#define O3D_BREAKPAD_WIN_EXCEPTION_HANDLER_WIN32_H_

namespace google_breakpad {
class ExceptionHandler;
};

// Sample usage:
//   int main(void) {
//     static ExceptionManager exception_manager(false);
//     exception_manager.StartMonitoring();
//     ...
//   }
class ExceptionManager {
 public:
  // If catch_entire_process is true, then all minidumps are captured.
  // Otherwise, only crashes in this module are captured.
  // Use the latter when running inside IE or Firefox.
  // StartMonitoring needs to be called before any minidumps are captured.
  explicit ExceptionManager(bool catch_entire_process);
  ~ExceptionManager();

  // Starts monitoring for crashes. When a crash occurs a minidump will
  // automatically be captured and sent.
  void StartMonitoring();

  // TODO: Cleanup. The following should not be called
  // directly, ideally these should be private methods.
  bool catch_entire_process() { return catch_entire_process_; }
  static void SendMinidump(const char *minidump_filename);
  static bool CanSendMinidump();  // considers throttling

 private:
  static ExceptionManager *instance_;

  bool catch_entire_process_;
  google_breakpad::ExceptionHandler *exception_handler_;
};

#endif  // O3D_BREAKPAD_WIN_EXCEPTION_HANDLER_WIN32_H_
