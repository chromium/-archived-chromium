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
#ifndef CHROME_TOOLS_CRASH_SERVICE__
#define CHROME_TOOLS_CRASH_SERVICE__

#include <string>

#include "base/basictypes.h"
#include "base/lock.h"

namespace google_breakpad {

class CrashReportSender;
class CrashGenerationServer;
class ClientInfo;

}

// This class implements an out-of-process crash server. It uses breakpad's
// CrashGenerationServer and CrashReportSender to generate and then send the
// crash dumps. Internally, it uses OS specific pipe to allow applications to
// register for crash dumps and later on when a registered application crashes
// it will signal an event that causes this code to wake up and perform a
// crash dump on the signaling process. The dump is then stored on disk and
// possibly sent to the crash2 servers.
class CrashService {
 public:
  // The ctor takes a directory that needs to be writable and will create
  // a subdirectory inside to keep logs, crashes and checkpoint files.
  CrashService(const std::wstring& report_dir);
  ~CrashService();

  // Starts servicing crash dumps. The command_line specifies various behaviors,
  // see below for more information. Returns false if it failed. Do not use
  // other members in that case.
  bool Initialize(const std::wstring& command_line);

  // Command line switches:
  //
  // --max-reports=<number>
  // Allows to override the maximum number for reports per day. Normally
  // the crash dumps are never sent so if you want to send any you must
  // specify a positive number here.
  static const wchar_t kMaxReports[];
  // --no-window
  // Does not create a visible window on the desktop. The window does not have
  // any other functionality other than allowing the crash service to be
  // gracefully closed.
  static const wchar_t kNoWindow[];
  // --reporter=<string>
  // Allows to specify a custom string that appears on the detail crash report
  // page in the crash server. This should be a 25 chars or less string.
  // The default tag if not specified is 'crash svc'.
  static const wchar_t kReporterTag[];

  // Returns the actual report path.
  std::wstring report_path() const {
    return report_path_;
  }
  // Returns number of crash dumps handled.
  int requests_handled() const {
    return requests_handled_;
  }
  // Returns number of crash clients registered.
  int clients_connected() const {
    return clients_connected_;
  }
  // Returns number of crash clients terminated.
  int clients_terminated() const {
    return clients_terminated_;
  }

  // Starts the processing loop. This function does not return unless the
  // user is logging off or the user closes the crash service window. The
  // return value is a good number to pass in ExitProcess().
  int ProcessingLoop();

 private:
  static void OnClientConnected(void* context,
                                const google_breakpad::ClientInfo* client_info);

  static void OnClientDumpRequest(void* context,
                                  const google_breakpad::ClientInfo* client_info,
                                  const std::wstring* file_path);

  static void OnClientExited(void* context,
                             const google_breakpad::ClientInfo* client_info);

  // This routine sends the crash dump to the server. It takes the sending_
  // lock when it is performing the send.
  static unsigned long __stdcall AsyncSendDump(void* context);

  google_breakpad::CrashGenerationServer* dumper_;
  google_breakpad::CrashReportSender* sender_;

  // the path to dumps and logs directory.
  std::wstring report_path_;
  // the extra tag sent to the server with each dump.
  std::wstring reporter_tag_;

  // clients serviced statistics:
  int requests_handled_;
  int requests_sent_;
  volatile long clients_connected_;
  volatile long clients_terminated_;
  Lock sending_;

  DISALLOW_EVIL_CONSTRUCTORS(CrashService);
};


#endif  // CHROME_TOOLS_CRASH_SERVICE__
