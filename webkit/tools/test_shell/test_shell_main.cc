// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Creates an instance of the test_shell.
#include "build/build_config.h"

#include <stdlib.h>  // required by _set_abort_behavior

#if defined(OS_WIN)
#include <windows.h>
#include <commctrl.h>
#include "base/event_recorder.h"
#include "base/gfx/native_theme.h"
#include "base/resource_util.h"
#include "base/win_util.h"
#include "webkit/tools/test_shell/foreground_helper.h"
#endif

#if defined(OS_LINUX)
#include <gtk/gtk.h>
#endif

#include "base/at_exit.h"
#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/icu_util.h"
#include "base/memory_debug.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/rand_util.h"
#include "base/stats_table.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "base/sys_info.h"
#include "base/trace_event.h"
#include "net/base/cookie_monster.h"
#include "net/base/net_module.h"
#include "net/http/http_cache.h"
#include "net/url_request/url_request_context.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/window_open_disposition.h"
#include "webkit/tools/test_shell/simple_resource_loader_bridge.h"
#include "webkit/tools/test_shell/test_shell.h"
#include "webkit/tools/test_shell/test_shell_request_context.h"
#include "webkit/tools/test_shell/test_shell_switches.h"

#include <iostream>
using namespace std;

static const size_t kPathBufSize = 2048;

namespace {

// StatsTable initialization parameters.
static const char* kStatsFilePrefix = "testshell_";
static int kStatsFileThreads = 20;
static int kStatsFileCounters = 200;

#if defined(OS_WIN)
StringPiece GetRawDataResource(HMODULE module, int resource_id) {
  void* data_ptr;
  size_t data_size;
  return base::GetDataResourceFromModule(module, resource_id, &data_ptr,
                                         &data_size) ?
      StringPiece(static_cast<char*>(data_ptr), data_size) : StringPiece();
}

// This is called indirectly by the network layer to access resources.
StringPiece NetResourceProvider(int key) {
  return GetRawDataResource(::GetModuleHandle(NULL), key);
}

// This test approximates whether you have the Windows XP theme selected by
// inspecting a couple of metrics. It does not catch all cases, but it does
// pick up on classic vs xp, and normal vs large fonts. Something it misses
// is changes to the color scheme (which will infact cause pixel test
// failures).
// 
// ** Expected dependencies **
// + Theme: Windows XP
// + Color scheme: Default (blue)
// + Font size: Normal
// + Font smoothing: off (minor impact).
// 
bool HasLayoutTestThemeDependenciesWin() {
  // This metric will be 17 when font size is "Normal". The size of drop-down
  // menus depends on it.
  if (::GetSystemMetrics(SM_CXVSCROLL) != 17)
    return false;

  // Check that the system fonts RenderThemeWin relies on are Tahoma 11 pt.
  NONCLIENTMETRICS metrics;
  win_util::GetNonClientMetrics(&metrics);
  LOGFONTW* system_fonts[] =
      { &metrics.lfStatusFont, &metrics.lfMenuFont, &metrics.lfSmCaptionFont };

  for (size_t i = 0; i < arraysize(system_fonts); ++i) {
    if (system_fonts[i]->lfHeight != -11 ||
        0 != wcscmp(L"Tahoma", system_fonts[i]->lfFaceName))
      return false;
  }
  return true;
}

bool CheckLayoutTestSystemDependenciesWin() {
  bool has_deps = HasLayoutTestThemeDependenciesWin();
  if (!has_deps) {
    fprintf(stderr, 
        "\n"
        "###############################################################\n"
        "## Layout test system dependencies check failed.\n"
        "## Some layout tests may fail due to unexpected theme.\n"
        "##\n"
        "## To fix, go to Display Properties -> Appearance, and select:\n"
        "##  + Windows and buttons: Windows XP style\n"
        "##  + Color scheme: Default (blue)\n"
        "##  + Font size: Normal\n"
        "###############################################################\n");
  }
  return has_deps;
}

#endif

bool CheckLayoutTestSystemDependencies() {
#if defined(OS_WIN)
  return CheckLayoutTestSystemDependenciesWin();
#else
  return true;
#endif
}

}  // namespace


int main(int argc, char* argv[]) {
  base::EnableTerminationOnHeapCorruption();
#ifdef _CRTDBG_MAP_ALLOC
  _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
  _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
#endif
  // Some tests may use base::Singleton<>, thus we need to instanciate
  // the AtExitManager or else we will leak objects.
  base::AtExitManager at_exit_manager;  

#if defined(OS_LINUX)
  gtk_init(&argc, &argv);
  // Only parse the command line after GTK's had a crack at it.
  CommandLine::SetArgcArgv(argc, argv);
#endif

  CommandLine parsed_command_line;
  if (parsed_command_line.HasSwitch(test_shell::kStartupDialog))
    TestShell::ShowStartupDebuggingDialog();

  if (parsed_command_line.HasSwitch(test_shell::kCheckLayoutTestSystemDeps)) {
    exit(CheckLayoutTestSystemDependencies() ? 0 : 1);
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
#if defined(OS_WIN)
  enable_gp_fault_error_box =
      parsed_command_line.HasSwitch(test_shell::kGPFaultErrorBox);
#endif
  TestShell::InitLogging(suppress_error_dialogs,
                         layout_test_mode,
                         enable_gp_fault_error_box);

  // Set this early before we start using WebCore.
  webkit_glue::SetLayoutTestMode(layout_test_mode);

  // Suppress abort message in v8 library in debugging mode.
  // V8 calls abort() when it hits assertion errors.
#if defined(OS_WIN)
  if (suppress_error_dialogs) {
    _set_abort_behavior(0, _WRITE_ABORT_MSG);
  }
#endif

  if (parsed_command_line.HasSwitch(test_shell::kEnableTracing))
    base::TraceLog::StartTracing();

  net::HttpCache::Mode cache_mode = net::HttpCache::NORMAL;
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

  std::wstring cache_path =
      parsed_command_line.GetSwitchValue(test_shell::kCacheDir);
  if (cache_path.empty()) {
    PathService::Get(base::DIR_EXE, &cache_path);
    file_util::AppendToPath(&cache_path, L"cache");
  }

  // Initializing with a default context, which means no on-disk cookie DB,
  // and no support for directory listings.
  SimpleResourceLoaderBridge::Init(
      new TestShellRequestContext(cache_path, cache_mode, layout_test_mode));

  // Load ICU data tables
  icu_util::Initialize();

#if defined(OS_WIN)
  // Config the network module so it has access to a limited set of resources.
  net::NetModule::SetResourceProvider(NetResourceProvider);

  INITCOMMONCONTROLSEX InitCtrlEx;

  InitCtrlEx.dwSize = sizeof(INITCOMMONCONTROLSEX);
  InitCtrlEx.dwICC  = ICC_STANDARD_CLASSES;
  InitCommonControlsEx(&InitCtrlEx);
#endif

  TestShell::InitializeTestShell(layout_test_mode);

  if (parsed_command_line.HasSwitch(test_shell::kAllowScriptsToCloseWindows))
    TestShell::SetAllowScriptsToCloseWindows();

  // Disable user themes for layout tests so pixel tests are consistent.
  if (layout_test_mode) {
#if defined(OS_WIN)
    gfx::NativeTheme::instance()->DisableTheming();
#elif defined(OS_LINUX)
    // Pick a theme that uses Cairo for drawing, since we:
    // 1) currently don't support GTK themes that use the GDK drawing APIs, and
    // 2) need to use a unified theme for layout tests anyway.
    g_object_set(gtk_settings_get_default(),
                 "gtk-theme-name", "Mist",
                 NULL);
#endif
  }

  if (parsed_command_line.HasSwitch(test_shell::kTestShellTimeOut)) {
    const std::wstring timeout_str = parsed_command_line.GetSwitchValue(
        test_shell::kTestShellTimeOut);
    int timeout_ms = static_cast<int>(StringToInt64(timeout_str.c_str()));
    if (timeout_ms > 0)
      TestShell::SetFileTestTimeout(timeout_ms);
  }

#if defined(OS_WIN)
  // Initialize global strings
  TestShell::RegisterWindowClass();
#endif

  // Treat the first loose value as the initial URL to open.
  std::wstring uri;

  // Default to a homepage if we're interactive.
  if (!layout_test_mode) {
    PathService::Get(base::DIR_SOURCE_ROOT, &uri);
    file_util::AppendToPath(&uri, L"webkit");
    file_util::AppendToPath(&uri, L"data");
    file_util::AppendToPath(&uri, L"test_shell");
    file_util::AppendToPath(&uri, L"index.html");
  }

  if (parsed_command_line.GetLooseValueCount() > 0) {
    CommandLine::LooseValueIterator iter(
        parsed_command_line.GetLooseValuesBegin());
    uri = *iter;
  }

  std::wstring js_flags = 
    parsed_command_line.GetSwitchValue(test_shell::kJavaScriptFlags);
  // Test shell always exposes the GC.
  CommandLine::AppendSwitch(&js_flags, L"expose-gc");
  webkit_glue::SetJavaScriptFlags(js_flags);
  // Also expose GCController to JavaScript.
  webkit_glue::SetShouldExposeGCController(true);

  // Load and initialize the stats table.  Attempt to construct a somewhat
  // unique name to isolate separate instances from each other.
  StatsTable *table = new StatsTable(
      kStatsFilePrefix + Uint64ToString(base::RandUint64()),
      kStatsFileThreads,
      kStatsFileCounters);
  StatsTable::set_current(table);

  TestShell* shell;
  if (TestShell::CreateNewWindow(uri, &shell)) {
#if defined(OS_WIN)
    if (record_mode || playback_mode) {
      // Move the window to the upper left corner for consistent
      // record/playback mode.  For automation, we want this to work
      // on build systems where the script invoking us is a background
      // process.  So for this case, make our window the topmost window
      // as well.
      ForegroundHelper::SetForeground(shell->mainWnd());
      ::SetWindowPos(shell->mainWnd(), HWND_TOP, 0, 0, 600, 800, 0);
      // Tell webkit as well.
      webkit_glue::SetRecordPlaybackMode(true);
    }
#endif

    shell->Show(shell->webView(), NEW_WINDOW);

    if (parsed_command_line.HasSwitch(test_shell::kDumpStatsTable))
      shell->DumpStatsTableOnExit();

#if defined(OS_WIN)
    bool no_events = parsed_command_line.HasSwitch(test_shell::kNoEvents);
    if ((record_mode || playback_mode) && !no_events) {
      std::wstring script_path = cache_path;
      // Create the cache directory in case it doesn't exist.
      file_util::CreateDirectory(cache_path);
      file_util::AppendToPath(&script_path, L"script.log");
      if (record_mode)
        base::EventRecorder::current()->StartRecording(script_path);
      if (playback_mode)
        base::EventRecorder::current()->StartPlayback(script_path);
    }
#endif

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

      if (uri.length() == 0) {
        // Watch stdin for URLs.
        char filenameBuffer[kPathBufSize];
        while (fgets(filenameBuffer, sizeof(filenameBuffer), stdin)) {
          char *newLine = strchr(filenameBuffer, '\n');
          if (newLine)
            *newLine = '\0';
          if (!*filenameBuffer)
            continue;


          if (!TestShell::RunFileTest(filenameBuffer, params))
            break;
        }
      } else {
        TestShell::RunFileTest(WideToUTF8(uri).c_str(), params);
      }

      shell->CallJSGC();
      shell->CallJSGC();
      if (shell) delete shell;
    } else {
      MessageLoop::current()->Run();
    }

    // Flush any remaining messages.  This ensures that any accumulated
    // Task objects get destroyed before we exit, which avoids noise in
    // purify leak-test results.
    MessageLoop::current()->RunAllPending();

#if defined(OS_WIN)
    if (record_mode)
      base::EventRecorder::current()->StopRecording();
    if (playback_mode)
      base::EventRecorder::current()->StopPlayback();
#endif
  }

  TestShell::ShutdownTestShell();
  TestShell::CleanupLogging();

  // Tear down shared StatsTable; prevents unit_tests from leaking it.
  StatsTable::set_current(NULL);
  delete table;

#ifdef _CRTDBG_MAP_ALLOC
  _CrtDumpMemoryLeaks();
#endif
  return 0;
}
