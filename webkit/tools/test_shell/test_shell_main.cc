// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Creates an instance of the test_shell.

#include <stdlib.h>  // required by _set_abort_behavior

#include <windows.h>
#include <commctrl.h>

#include "base/at_exit.h"
#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/event_recorder.h"
#include "base/file_util.h"
#include "base/gfx/native_theme.h"
#include "base/icu_util.h"
#include "base/memory_debug.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/resource_util.h"
#include "base/stack_container.h"
#include "base/stats_table.h"
#include "base/string_util.h"
#include "base/trace_event.h"
#include "breakpad/src/client/windows/handler/exception_handler.h"
#include "net/base/cookie_monster.h"
#include "net/base/net_module.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_layer.h"
#include "net/url_request/url_request_context.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/window_open_disposition.h"
#include "webkit/tools/test_shell/foreground_helper.h"
#include "webkit/tools/test_shell/simple_resource_loader_bridge.h"
#include "webkit/tools/test_shell/test_shell.h"
#include "webkit/tools/test_shell/test_shell_request_context.h"
#include "webkit/tools/test_shell/test_shell_switches.h"

// This is only set for layout tests.
static wchar_t g_currentTestName[MAX_PATH];

namespace {

// StatsTable initialization parameters.
static wchar_t* kStatsFile = L"testshell";
static int kStatsFileThreads = 20;
static int kStatsFileCounters = 200;

std::string GetDataResource(HMODULE module, int resource_id) {
  void* data_ptr;
  size_t data_size;
  return base::GetDataResourceFromModule(module, resource_id, &data_ptr,
                                         &data_size) ?
      std::string(static_cast<char*>(data_ptr), data_size) : std::string();
}

// This is called indirectly by the network layer to access resources.
std::string NetResourceProvider(int key) {
  return GetDataResource(::GetModuleHandle(NULL), key);
}

void SetCurrentTestName(char* path)
{
    char* lastSlash = strrchr(path, '/');
    if (lastSlash) {
        ++lastSlash;
    } else {
        lastSlash = path;
    }

    wcscpy_s(g_currentTestName, arraysize(g_currentTestName),
             UTF8ToWide(lastSlash).c_str());
}

bool MinidumpCallback(const wchar_t *dumpPath,
                             const wchar_t *minidumpID,
                             void *context,
                             EXCEPTION_POINTERS *exinfo,
                             MDRawAssertionInfo *assertion,
                             bool succeeded)
{
    // Warning: Don't use the heap in this function.  It may be corrupted.
    if (!g_currentTestName[0])
        return false;

    // Try to rename the minidump file to include the crashed test's name.
    // StackString uses the stack but overflows onto the heap.  But we don't
    // care too much about being completely correct here, since most crashes
    // will be happening on developers' machines where they have debuggers.
    StackWString<MAX_PATH*2> origPath;
    origPath->append(dumpPath);
    origPath->push_back(file_util::kPathSeparator);
    origPath->append(minidumpID);
    origPath->append(L".dmp");

    StackWString<MAX_PATH*2>  newPath;
    newPath->append(dumpPath);
    newPath->push_back(file_util::kPathSeparator);
    newPath->append(g_currentTestName);
    newPath->append(L"-");
    newPath->append(minidumpID);
    newPath->append(L".dmp");

    // May use the heap, but oh well.  If this fails, we'll just have the
    // original dump file lying around.
    _wrename(origPath->c_str(), newPath->c_str());

    return false;
}
}  // namespace

int main(int argc, char* argv[]) {
  process_util::EnableTerminationOnHeapCorruption();
#ifdef _CRTDBG_MAP_ALLOC
  _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
  _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
#endif
  // Some tests may use base::Singleton<>, thus we need to instanciate
  // the AtExitManager or else we will leak objects.
  base::AtExitManager at_exit_manager;  

  CommandLine parsed_command_line;
  if (parsed_command_line.HasSwitch(test_shell::kStartupDialog))
      MessageBox(NULL, L"attach to me?", L"test_shell", MB_OK);
  //webkit_glue::SetLayoutTestMode(true);

  // Allocate a message loop for this thread.  Although it is not used
  // directly, its constructor sets up some necessary state.
  MessageLoopForUI main_message_loop;

  bool suppress_error_dialogs = 
       (GetEnvironmentVariable(L"CHROME_HEADLESS", NULL, 0) ||
       parsed_command_line.HasSwitch(test_shell::kNoErrorDialogs) ||
       parsed_command_line.HasSwitch(test_shell::kLayoutTests));
  bool layout_test_mode =
      parsed_command_line.HasSwitch(test_shell::kLayoutTests);

  TestShell::InitLogging(suppress_error_dialogs, layout_test_mode);

  // Suppress abort message in v8 library in debugging mode.
  // V8 calls abort() when it hits assertion errors.
  if (suppress_error_dialogs) {
    _set_abort_behavior(0, _WRITE_ABORT_MSG);
  }

  if (parsed_command_line.HasSwitch(test_shell::kEnableTracing))
    base::TraceLog::StartTracing();

#if defined(OS_WIN)
  // Make the selection of network stacks early on before any consumers try to
  // issue HTTP requests.
  if (parsed_command_line.HasSwitch(test_shell::kUseWinHttp))
    net::HttpNetworkLayer::UseWinHttp(true);
#endif

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
      new TestShellRequestContext(cache_path, cache_mode));

  // Load ICU data tables
  icu_util::Initialize();

  // Config the network module so it has access to a limited set of resources.
  net::NetModule::SetResourceProvider(NetResourceProvider);

  INITCOMMONCONTROLSEX InitCtrlEx;

  InitCtrlEx.dwSize = sizeof(INITCOMMONCONTROLSEX);
  InitCtrlEx.dwICC  = ICC_STANDARD_CLASSES;
  InitCommonControlsEx(&InitCtrlEx);

  // Register the Ahem font used by layout tests.
  DWORD num_fonts = 1;
  void* font_ptr;
  size_t font_size;
  if (base::GetDataResourceFromModule(::GetModuleHandle(NULL), IDR_AHEM_FONT, 
                                      &font_ptr, &font_size)) {
    HANDLE rc = AddFontMemResourceEx(font_ptr, font_size, 0, &num_fonts);
    DCHECK(rc != 0);
  }

  bool interactive = !layout_test_mode;
  TestShell::InitializeTestShell(interactive);

  if (parsed_command_line.HasSwitch(test_shell::kAllowScriptsToCloseWindows))
    TestShell::SetAllowScriptsToCloseWindows();

  // Disable user themes for layout tests so pixel tests are consistent.
  if (!interactive)
    gfx::NativeTheme::instance()->DisableTheming();

  if (parsed_command_line.HasSwitch(test_shell::kTestShellTimeOut)) {
    const std::wstring timeout_str = parsed_command_line.GetSwitchValue(
        test_shell::kTestShellTimeOut);
    int timeout_ms = static_cast<int>(StringToInt64(timeout_str.c_str()));
    if (timeout_ms > 0)
      TestShell::SetFileTestTimeout(timeout_ms);
  }

  // Initialize global strings
  TestShell::RegisterWindowClass();

  // Treat the first loose value as the initial URL to open.
  std::wstring uri;

  // Default to a homepage if we're interactive.
  if (interactive) {
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

  if (parsed_command_line.HasSwitch(test_shell::kCrashDumps)) {
    std::wstring dir(
        parsed_command_line.GetSwitchValue(test_shell::kCrashDumps));
    new google_breakpad::ExceptionHandler(dir, 0, &MinidumpCallback, 0, true);
  }

  std::wstring js_flags = 
    parsed_command_line.GetSwitchValue(test_shell::kJavaScriptFlags);
  // Test shell always exposes the GC.
  CommandLine::AppendSwitch(&js_flags, L"expose-gc");
  webkit_glue::SetJavaScriptFlags(js_flags);

  // load and initialize the stats table.
  StatsTable *table = new StatsTable(kStatsFile, kStatsFileThreads, kStatsFileCounters);
  StatsTable::set_current(table);

  TestShell* shell;
  if (TestShell::CreateNewWindow(uri, &shell)) {
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

    shell->Show(shell->webView(), NEW_WINDOW);

    if (parsed_command_line.HasSwitch(test_shell::kDumpStatsTable))
      shell->DumpStatsTableOnExit();

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

    if (parsed_command_line.HasSwitch(test_shell::kDebugMemoryInUse)) {
      base::MemoryDebug::SetMemoryInUseEnabled(true);
      // Dump all in use memory at startup
      base::MemoryDebug::DumpAllMemoryInUse();
    }

    // See if we need to run the tests.
    if (layout_test_mode) {
      webkit_glue::SetLayoutTestMode(true);

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
        char filenameBuffer[2048];
        while (fgets(filenameBuffer, sizeof(filenameBuffer), stdin)) {
          char *newLine = strchr(filenameBuffer, '\n');
          if (newLine)
            *newLine = '\0';
          if (!*filenameBuffer)
            continue;

          SetCurrentTestName(filenameBuffer);

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

#ifdef _CRTDBG_MAP_ALLOC
  _CrtDumpMemoryLeaks();
#endif
  return 0;
}
