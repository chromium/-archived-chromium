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

#include <windows.h>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/perftimer.h"
#include "base/string_util.h"
#include "base/test_suite.h"

int main(int argc, char** argv) {
  // Some tests may use base::Singleton<>, thus we need to instanciate
  // the AtExitManager or else we will leak objects.
  base::AtExitManager at_exit_manager;  

  // Initialize the perf timer log
  std::string log_file =
      WideToUTF8(CommandLine().GetSwitchValue(L"log-file"));
  if (log_file.empty())
    log_file = "perf_test.log";
  ASSERT_TRUE(InitPerfLog(log_file.c_str()));

  // Raise to high priority to have more precise measurements. Since we don't
  // aim at 1% precision, it is not necessary to run at realtime level.
  if (!IsDebuggerPresent())
    SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);

  int rv = TestSuite(argc, argv).Run();

  FinalizePerfLog();
  return rv;
}
