// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>
#include <sys/syslimits.h>
#include <unistd.h>
#import <mach/task.h>

#include <string>

#include "base/at_exit.h"
#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/icu_util.h"
#include "base/memory_debug.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "net/base/cookie_monster.h"
#include "net/http/http_cache.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/tools/test_shell/simple_resource_loader_bridge.h"
#include "webkit/tools/test_shell/test_shell.h"
#include "webkit/tools/test_shell/test_shell_request_context.h"
#include "webkit/tools/test_shell/test_shell_switches.h"

#include "WebSystemInterface.h"

static char g_currentTestName[PATH_MAX];

static const char* kStatsFile = "testshell";
static int kStatsFileThreads = 20;
static int kStatsFileCounters = 100;

// Extracts the name of the test from the given path and sets the test name
// global.
static void SetCurrentTestName(char* path) {
  char* lastSlash = strrchr(path, '/');
  if (lastSlash) {
    ++lastSlash;
  } else {
    lastSlash = path;
  }
  
  strncpy(g_currentTestName, lastSlash, PATH_MAX);
  g_currentTestName[PATH_MAX-1] = '\0';
}

// The application delegate, used to hook application termination so that we
// can kill the TestShell object and do some other app-wide cleanup. Once we
// go into the run-loop, we never come back to main.
@interface TestShellAppDelegate : NSObject {
 @private
  TestShell* shell_;  // strong
}
- (id)initWithShell:(TestShell*)shell;
@end

@implementation TestShellAppDelegate
- (id)initWithShell:(TestShell*)shell {
  if ((self = [super init])) {
    shell_ = shell;
  }
  return self;
}

- (void)dealloc {
  // Flush any remaining messages.  This ensures that any accumulated
  // Task objects get destroyed before we exit, which avoids noise in
  // purify leak-test results.
  MessageLoop::current()->RunAllPending();
  
  TestShell::ShutdownTestShell();

  // get rid of the stats table last, V8 relies on it
  StatsTable* table = StatsTable::current();
  StatsTable::set_current(NULL);
  delete table;

  TestShell::CleanupLogging();

  [super dealloc];
}

// Called because we're the NSApp's delegate. Destroy ourselves which forces
// shutdown cleanup to be called.
- (void)applicationWillTerminate:(id)sender {
  // commit suicide.
  [self release];
}

@end

int main(const int argc, const char *argv[]) {
  InitWebCoreSystemInterface();

  // Some tests may use base::Singleton<>, thus we need to instantiate
  // the AtExitManager or else we will leak objects.
  base::AtExitManager at_exit_manager;  

  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

  // Allocate a message loop for this thread.  Although it is not used
  // directly, its constructor sets up some necessary state.
  MessageLoop main_message_loop(MessageLoop::TYPE_UI);

  // Force AppKit to init itself, but don't start the runloop yet
  [NSApplication sharedApplication];
  [NSBundle loadNibNamed:@"MainMenu" owner:NSApp];

  // Interpret the same flags as
  // the windows version, so that we can run the same test scripts. stop
  // if we hit something that's not a switch (like, oh, a URL).

  CommandLine::SetArgcArgv(argc, argv);
  CommandLine parsed_command_line(argc, argv);

  if (parsed_command_line.HasSwitch(test_shell::kCheckLayoutTestSystemDeps)) {
    // Always succeed the deps check, currently just used by windows port.
    exit(0);
  }
  
  if (parsed_command_line.HasSwitch(test_shell::kStartupDialog)) {
    //TODO: add alert to allow attaching via gdb before things really get going
  }

  if (parsed_command_line.HasSwitch(test_shell::kCrashDumps)) {
      std::wstring dir =
          parsed_command_line.GetSwitchValue(test_shell::kCrashDumps);
      // TODO: enable breakpad
      // new google_breakpad::ExceptionHandler(dir, 0, &MinidumpCallback, 0, true);
  }

  bool suppress_error_dialogs = (getenv("CHROME_HEADLESS") != NULL) ||
      parsed_command_line.HasSwitch(test_shell::kNoErrorDialogs) ||
      parsed_command_line.HasSwitch(test_shell::kLayoutTests);
  bool layout_test_mode =
      parsed_command_line.HasSwitch(test_shell::kLayoutTests);
  
  TestShell::InitLogging(suppress_error_dialogs, layout_test_mode, false);
  TestShell::InitializeTestShell(layout_test_mode);

  bool no_tree = parsed_command_line.HasSwitch(test_shell::kNoTree);
  
  bool dump_pixels = parsed_command_line.HasSwitch(test_shell::kDumpPixels);
  std::wstring pixel_file_name;
  if (dump_pixels) {
    pixel_file_name =
        parsed_command_line.GetSwitchValue(test_shell::kDumpPixels);
    if (pixel_file_name.size() == 0) {
      fprintf(stderr, "No file specified for pixel tests");
      exit(1);
    }
  }

  if (parsed_command_line.HasSwitch(test_shell::kTestShellTimeOut)) {
    const std::wstring timeout_str = parsed_command_line.GetSwitchValue(
        test_shell::kTestShellTimeOut);
    int timeout_ms = static_cast<int>(StringToInt64(timeout_str.c_str()));
    if (timeout_ms > 0)
      TestShell::SetFileTestTimeout(timeout_ms);
  }

  std::wstring javascript_flags =
      parsed_command_line.GetSwitchValue(test_shell::kJavaScriptFlags);
  // Test shell always exposes the GC.
  CommandLine::AppendSwitch(&javascript_flags, L"expose-gc");
  webkit_glue::SetJavaScriptFlags(javascript_flags);

  // Load and initialize the stats table (one per process, so that multiple
  // instances don't interfere with each other)
  char statsfile[64];
  snprintf(statsfile, 64, "%s-%d", kStatsFile, getpid());
  StatsTable *table = 
      new StatsTable(statsfile, kStatsFileThreads, kStatsFileCounters);
  StatsTable::set_current(table);

  net::HttpCache::Mode cache_mode = net::HttpCache::NORMAL;
  bool playback_mode = 
    parsed_command_line.HasSwitch(test_shell::kPlaybackMode);
  bool record_mode = 
    parsed_command_line.HasSwitch(test_shell::kRecordMode);

  if (playback_mode)
    cache_mode = net::HttpCache::PLAYBACK;
  else if (record_mode)
    cache_mode = net::HttpCache::RECORD;

  bool dump_stats_table =
      parsed_command_line.HasSwitch(test_shell::kDumpStatsTable);

  bool debug_memory_in_use =
      parsed_command_line.HasSwitch(test_shell::kDebugMemoryInUse);  
  
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
  
  // Config the network module so it has access to a limited set of resources.
  // NetModule::SetResourceProvider(NetResourceProvider);
  
  // Treat the first loose value as the initial URL to open.
  std::wstring uri;

  // Default to a homepage if we're interactive
  if (!layout_test_mode) {
    NSString *resourcePath = [[NSBundle mainBundle] resourcePath];
    NSString *testShellPath =
        [resourcePath stringByAppendingPathComponent:@"test_shell/index.html"];
    // don't use NSURL; it puts a "localhost" bit in that freaks out our URL
    // handling
    NSString *testShellURL = [NSString stringWithFormat:@"file://%@",
                              testShellPath];
    uri = UTF8ToWide([testShellURL UTF8String]);
  }

  if (parsed_command_line.GetLooseValueCount() > 0) {
    CommandLine::LooseValueIterator iter =
        parsed_command_line.GetLooseValuesBegin();
    uri = *iter;
  }
  
  TestShell* shell;
  if (TestShell::CreateNewWindow(uri, &shell)) {
#ifdef NOTYET
    if (record_mode || playback_mode) {
      // Move the window to the upper left corner for consistent
      // record/playback mode.  For automation, we want this to work
      // on build systems where the script invoking us is a background
      // process.  So for this case, make our window the topmost window
      // as well.
      // ForegroundHelper::SetForeground(shell->mainWnd());
      // ::SetWindowPos(shell->mainWnd(), HWND_TOP, 0, 0, 600, 800, 0);
      // Tell webkit as well.
      webkit_glue::SetRecordPlaybackMode(true);
    }
#endif
    shell->Show(shell->webView(), NEW_WINDOW);
    
    if (dump_stats_table)
      shell->DumpStatsTableOnExit();

#ifdef NOTYET
    if ((record_mode || playback_mode) && !no_events) {
      std::string script_path = cache_path;
      // Create the cache directory in case it doesn't exist.
      file_util::CreateDirectory(cache_path);
      file_util::AppendToPath(&script_path, "script.log");
      if (record_mode)
        base::EventRecorder::current()->StartRecording(script_path);
      if (playback_mode)
        base::EventRecorder::current()->StartPlayback(script_path);
    }
#endif
    
    if (debug_memory_in_use) {
      base::MemoryDebug::SetMemoryInUseEnabled(true);
      base::MemoryDebug::DumpAllMemoryInUse();
    }

    // Set up our app delegate so we can tear down the TestShell object when
    // necessary. |delegate| takes ownership of |shell|, and will clean itself
    // up when it receives the notification that the app is terminating.
    TestShellAppDelegate* delegate = [[TestShellAppDelegate alloc] 
                                      initWithShell:shell];
    [[NSApplication sharedApplication] setDelegate:delegate];
    
    if (layout_test_mode) {
      // If we die during tests, we don't want to be spamming the user's crash
      // reporter. Set our exception port to null.
      if (task_set_exception_ports(mach_task_self(),
                                   EXC_MASK_ALL,
                                   MACH_PORT_NULL,
                                   EXCEPTION_DEFAULT,
                                   THREAD_STATE_NONE) != KERN_SUCCESS) {
        return -1;
      }
      
      // Cocoa housekeeping
      [NSApp finishLaunching];
      webkit_glue::SetLayoutTestMode(true);
      
      // Set up for the kind of test requested.
      TestShell::TestParams params;
      if (dump_pixels) {
        // The pixel test flag also gives the image file name to use.
        params.dump_pixels = true;
        params.pixel_file_name = pixel_file_name;
        if (params.pixel_file_name.size() == 0) {
          fprintf(stderr, "No file specified for pixel tests");
          exit(1);
        }
      }
      if (no_tree)
        params.dump_tree = false;
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
    } else {
      main_message_loop.Run();
    }

#ifdef NOTYET
    if (record_mode)
      base::EventRecorder::current()->StopRecording();
    if (playback_mode)
      base::EventRecorder::current()->StopPlayback();
#endif
  }

  [pool release];
  return 0;
}
