// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>

#include "base/at_exit.h"
#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/event_recorder.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/icu_util.h"
#include "base/memory_debug.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/rand_util.h"
#include "base/stats_table.h"
#include "base/string_util.h"
#include "base/sys_info.h"
#include "base/trace_event.h"
#include "net/base/cookie_monster.h"
#include "net/base/net_module.h"
#include "net/http/http_cache.h"
#include "net/socket/ssl_test_util.h"
#include "net/url_request/url_request_context.h"
#include "webkit/api/public/WebKit.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/window_open_disposition.h"
#include "webkit/extensions/v8/gc_extension.h"
#include "webkit/extensions/v8/playback_extension.h"
#include "webkit/extensions/v8/profiler_extension.h"
#include "webkit/tools/test_shell/simple_resource_loader_bridge.h"
#include "webkit/tools/test_shell/test_shell.h"
#include "webkit/tools/test_shell/test_shell_platform_delegate.h"
#include "webkit/tools/test_shell/test_shell_request_context.h"
#include "webkit/tools/test_shell/test_shell_switches.h"
#include "webkit/tools/test_shell/test_shell_webkit_init.h"

using namespace std;

static const size_t kPathBufSize = 2048;

namespace {

// StatsTable initialization parameters.
static const char* kStatsFilePrefix = "testshell_";
static int kStatsFileThreads = 20;
static int kStatsFileCounters = 200;

}  // namespace

int main(int argc, char* argv[]) {
  base::EnableTerminationOnHeapCorruption();

  // Some tests may use base::Singleton<>, thus we need to instanciate
  // the AtExitManager or else we will leak objects.
  base::AtExitManager at_exit_manager;

  TestShellPlatformDelegate::PreflightArgs(&argc, &argv);
  CommandLine::Init(argc, argv);
  const CommandLine& parsed_command_line = *CommandLine::ForCurrentProcess();

  TestShellPlatformDelegate platform(parsed_command_line);

  if (parsed_command_line.HasSwitch(test_shell::kStartupDialog))
    TestShell::ShowStartupDebuggingDialog();

  if (parsed_command_line.HasSwitch(test_shell::kCheckLayoutTestSystemDeps)) {
    exit(platform.CheckLayoutTestSystemDependencies() ? 0 : 1);
  }

  // Allocate a message loop for this thread.  Although it is not used
  // directly, its constructor sets up some necessary state.
  MessageLoopForUI main_message_loop;

  bool suppress_error_dialogs = (
       base::SysInfo::HasEnvVar(L"CHROME_HEADLESS") ||
       parsed_command_line.HasSwitch(test_shell::kNoErrorDialogs) ||
       parsed_command_line.HasSwitch(test_shell::kLayoutTests));
  bool layout_test_mode =
      parsed_command_line.HasSwitch(test_shell::kLayoutTests);

  bool enable_gp_fault_error_box = false;
  enable_gp_fault_error_box =
      parsed_command_line.HasSwitch(test_shell::kGPFaultErrorBox);
  TestShell::InitLogging(suppress_error_dialogs,
                         layout_test_mode,
                         enable_gp_fault_error_box);

  // Initialize WebKit for this scope.
  TestShellWebKitInit test_shell_webkit_init(layout_test_mode);

  // Suppress abort message in v8 library in debugging mode (but not
  // actually under a debugger).  V8 calls abort() when it hits
  // assertion errors.
  if (suppress_error_dialogs) {
    platform.SuppressErrorReporting();
  }

  if (parsed_command_line.HasSwitch(test_shell::kEnableTracing))
    base::TraceLog::StartTracing();

  net::HttpCache::Mode cache_mode = net::HttpCache::NORMAL;

  // This is a special mode where JS helps the browser implement
  // playback/record mode.  Generally, in this mode, some functions
  // of client-side randomness are removed.  For example, in
  // this mode Math.random() and Date.getTime() may not return
  // values which vary.
  bool playback_mode =
    parsed_command_line.HasSwitch(test_shell::kPlaybackMode);
  bool record_mode =
    parsed_command_line.HasSwitch(test_shell::kRecordMode);

  if (playback_mode)
    cache_mode = net::HttpCache::PLAYBACK;
  else if (record_mode)
    cache_mode = net::HttpCache::RECORD;

  if (layout_test_mode ||
      parsed_command_line.HasSwitch(test_shell::kEnableFileCookies))
    net::CookieMonster::EnableFileScheme();

  FilePath cache_path = FilePath::FromWStringHack(
      parsed_command_line.GetSwitchValue(test_shell::kCacheDir));
  // If the cache_path is empty and it's layout_test_mode, leave it empty
  // so we use an in-memory cache. This makes running multiple test_shells
  // in parallel less flaky.
  if (cache_path.empty() && !layout_test_mode) {
    PathService::Get(base::DIR_EXE, &cache_path);
    cache_path = cache_path.AppendASCII("cache");
  }

  // Initializing with a default context, which means no on-disk cookie DB,
  // and no support for directory listings.
  SimpleResourceLoaderBridge::Init(
      new TestShellRequestContext(cache_path.ToWStringHack(),
                                  cache_mode, layout_test_mode));

  // Load ICU data tables
  icu_util::Initialize();

  // Config the network module so it has access to a limited set of resources.
  net::NetModule::SetResourceProvider(TestShell::NetResourceProvider);

  // On Linux, load the test root certificate.
  net::TestServerLauncher ssl_util;
  ssl_util.LoadTestRootCert();

  platform.InitializeGUI();

  TestShell::InitializeTestShell(layout_test_mode);

  if (parsed_command_line.HasSwitch(test_shell::kAllowScriptsToCloseWindows))
    TestShell::SetAllowScriptsToCloseWindows();

  // Disable user themes for layout tests so pixel tests are consistent.
  if (layout_test_mode) {
    platform.SelectUnifiedTheme();
  }

  if (parsed_command_line.HasSwitch(test_shell::kTestShellTimeOut)) {
    const std::wstring timeout_str = parsed_command_line.GetSwitchValue(
        test_shell::kTestShellTimeOut);
    int timeout_ms =
        static_cast<int>(StringToInt64(WideToUTF16Hack(timeout_str.c_str())));
    if (timeout_ms > 0)
      TestShell::SetFileTestTimeout(timeout_ms);
  }

  // Treat the first loose value as the initial URL to open.
  FilePath uri;

  // Default to a homepage if we're interactive.
  if (!layout_test_mode) {
    PathService::Get(base::DIR_SOURCE_ROOT, &uri);
    uri = uri.AppendASCII("webkit");
    uri = uri.AppendASCII("data");
    uri = uri.AppendASCII("test_shell");
    uri = uri.AppendASCII("index.html");
  }

  std::vector<std::wstring> loose_values = parsed_command_line.GetLooseValues();
  if (loose_values.size() > 0)
    uri = FilePath::FromWStringHack(loose_values[0]);

  std::wstring js_flags =
    parsed_command_line.GetSwitchValue(test_shell::kJavaScriptFlags);
  // Test shell always exposes the GC.
  js_flags += L" --expose-gc";
  webkit_glue::SetJavaScriptFlags(js_flags);
  // Expose GCController to JavaScript.
  WebKit::registerExtension(extensions_v8::GCExtension::Get());

  if (parsed_command_line.HasSwitch(test_shell::kProfiler)) {
    WebKit::registerExtension(extensions_v8::ProfilerExtension::Get());
  }

  // Load and initialize the stats table.  Attempt to construct a somewhat
  // unique name to isolate separate instances from each other.
  StatsTable *table = new StatsTable(
      // truncate the random # to 32 bits for the benefit of Mac OS X, to
      // avoid tripping over its maximum shared memory segment name length
      kStatsFilePrefix + Uint64ToString(base::RandUint64() & 0xFFFFFFFFL),
      kStatsFileThreads,
      kStatsFileCounters);
  StatsTable::set_current(table);

  TestShell* shell;
  if (TestShell::CreateNewWindow(uri.ToWStringHack(), &shell)) {
    if (record_mode || playback_mode) {
      platform.SetWindowPositionForRecording(shell);
      WebKit::registerExtension(extensions_v8::PlaybackExtension::Get());
    }

    shell->Show(shell->webView(), NEW_WINDOW);

    if (parsed_command_line.HasSwitch(test_shell::kDumpStatsTable))
      shell->DumpStatsTableOnExit();

    bool no_events = parsed_command_line.HasSwitch(test_shell::kNoEvents);
    if ((record_mode || playback_mode) && !no_events) {
      FilePath script_path = cache_path;
      // Create the cache directory in case it doesn't exist.
      file_util::CreateDirectory(cache_path);
      script_path = script_path.AppendASCII("script.log");
      if (record_mode)
        base::EventRecorder::current()->StartRecording(script_path);
      if (playback_mode)
        base::EventRecorder::current()->StartPlayback(script_path);
    }

    if (parsed_command_line.HasSwitch(test_shell::kDebugMemoryInUse)) {
      base::MemoryDebug::SetMemoryInUseEnabled(true);
      // Dump all in use memory at startup
      base::MemoryDebug::DumpAllMemoryInUse();
    }

    // See if we need to run the tests.
    if (layout_test_mode) {
      // Set up for the kind of test requested.
      TestShell::TestParams params;
      if (parsed_command_line.HasSwitch(test_shell::kDumpPixels)) {
        // The pixel test flag also gives the image file name to use.
        params.dump_pixels = true;
        params.pixel_file_name = parsed_command_line.GetSwitchValue(
            test_shell::kDumpPixels);
        if (params.pixel_file_name.size() == 0) {
          fprintf(stderr, "No file specified for pixel tests");
          exit(1);
        }
      }
      if (parsed_command_line.HasSwitch(test_shell::kNoTree)) {
          params.dump_tree = false;
      }

      if (uri.empty()) {
        // Watch stdin for URLs.
        char filenameBuffer[kPathBufSize];
        while (fgets(filenameBuffer, sizeof(filenameBuffer), stdin)) {
          // When running layout tests we pass new line separated
          // tests to TestShell. Each line is a space separated list
          // of filename, timeout and expected pixel hash. The timeout
          // and the pixel hash are optional.
          char* newLine = strchr(filenameBuffer, '\n');
          if (newLine)
            *newLine = '\0';
          if (!*filenameBuffer)
            continue;

          params.test_url = strtok(filenameBuffer, " ");

          int old_timeout_ms = TestShell::GetLayoutTestTimeout();

          char* timeout = strtok(NULL, " ");
          if (timeout) {
            TestShell::SetFileTestTimeout(atoi(timeout));
            char* pixel_hash = strtok(NULL, " ");
            if (pixel_hash)
              params.pixel_hash = pixel_hash;
          }

          if (!TestShell::RunFileTest(params))
            break;

          TestShell::SetFileTestTimeout(old_timeout_ms);
        }
      } else {
        // TODO(ojan): Provide a way for run-singly tests to pass
        // in a hash and then set params.pixel_hash here.
        params.test_url = WideToUTF8(uri.ToWStringHack()).c_str();
        TestShell::RunFileTest(params);
      }

      shell->CallJSGC();
      shell->CallJSGC();

      // When we finish the last test, cleanup the LayoutTestController.
      // It may have references to not-yet-cleaned up windows.  By
      // cleaning up here we help purify reports.
      shell->ResetTestController();

      // Flush any remaining messages before we kill ourselves.
      // http://code.google.com/p/chromium/issues/detail?id=9500
      MessageLoop::current()->RunAllPending();

      delete shell;
    } else {
      MessageLoop::current()->Run();
    }

    // Flush any remaining messages.  This ensures that any accumulated
    // Task objects get destroyed before we exit, which avoids noise in
    // purify leak-test results.
    MessageLoop::current()->RunAllPending();

    if (record_mode)
      base::EventRecorder::current()->StopRecording();
    if (playback_mode)
      base::EventRecorder::current()->StopPlayback();
  }

  TestShell::ShutdownTestShell();
  TestShell::CleanupLogging();

  // Tear down shared StatsTable; prevents unit_tests from leaking it.
  StatsTable::set_current(NULL);
  delete table;

  return 0;
}
