// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
// running file tests in layout test mode, in ms.
const wchar_t kTestShellTimeOut[] = L"time-out-ms";

const wchar_t kStartupDialog[] = L"testshell-startup-dialog";

// Enable the Windows dialogs for GP faults in the test shell. This allows makes
// it possible to attach a crashed test shell to a debugger.
const wchar_t kGPFaultErrorBox[] = L"gp-fault-error-box";

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

// Enable the winhttp network stack.
const wchar_t kUseWinHttp[] = L"winhttp";

// Enable tracing events (see base/trace_event.h)
const wchar_t kEnableTracing[] = L"enable-tracing";

// Allow scripts to close windows in all cases.
const wchar_t kAllowScriptsToCloseWindows[] = L"allow-scripts-to-close-windows";

// Test the system dependencies (themes, fonts, ...). When this flag is
// specified, the test shell will exit immediately with either 0 (success) or
// 1 (failure). Combining with other flags has no effect.
extern const wchar_t kCheckLayoutTestSystemDeps[] =
    L"check-layout-test-sys-deps";

// Enable the media player by having this switch.
extern const wchar_t kEnableVideo[] = L"enable-video";

}  // namespace test_shell

