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

#include "webkit/tools/test_shell/test_shell_switches.h"

namespace test_shell {

// Suppresses all error dialogs when present.
const wchar_t kNoErrorDialogs[]                = L"noerrdialogs";

// Causes the test_shell to run using stdin and stdout for URLs and output,
// respectively, and interferes with interactive use of the UI.
const wchar_t kLayoutTests[] = L"layout-tests";
const wchar_t kCrashDumps[] = L"crash-dumps";  // Enable crash dumps

// Command line flags that control the tests when layout-tests is specified.
const wchar_t kNoTree[] = L"notree";  // Don't dump the render tree.
const wchar_t kDumpPixels[] = L"pixel-tests";  // Enable pixel tests.
// Optional command line switch that specifies timeout time for page load when
// running non-interactive file tests, in ms.
const wchar_t kTestShellTimeOut[] = L"time-out-ms";

const wchar_t kStartupDialog[] = L"testshell-startup-dialog";

// JavaScript flags passed to engine.
const wchar_t kJavaScriptFlags[] = L"js-flags";

// Run the http cache in record mode.
const wchar_t kRecordMode[] = L"record-mode";

// Run the http cache in playback mode.
const wchar_t kPlaybackMode[] = L"playback-mode";

// Don't record/playback events when using record & playback.
const wchar_t kNoEvents[] = L"no-events";

// Dump stats table on exit.
const wchar_t kDumpStatsTable[] = L"stats";

// Use a specified cache directory.
const wchar_t kCacheDir[] = L"cache-dir";

// When being run through a memory profiler, trigger memory in use dumps at 
// startup and just prior to shutdown.
const wchar_t kDebugMemoryInUse[] = L"debug-memory-in-use";

// Enable cookies on the file:// scheme.  --layout-tests also enables this.
const wchar_t kEnableFileCookies[] = L"enable-file-cookies";

}  // namespace test_shell
