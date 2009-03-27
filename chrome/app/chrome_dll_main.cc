// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(port): the ifdefs in here are a first step towards trying to determine
// the correct abstraction for all the OS functionality required at this
// stage of process initialization. It should not be taken as a final
// abstraction.

#include "build/build_config.h"

#if defined(OS_WIN)
#include <algorithm>
#include <atlbase.h>
#include <atlapp.h>
#include <malloc.h>
#include <new.h>
#endif

#if defined(OS_LINUX)
#include <gtk/gtk.h>
#endif

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/debug_util.h"
#include "base/icu_util.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/scoped_nsautorelease_pool.h"
#include "base/stats_table.h"
#include "base/string_util.h"
#if defined(OS_WIN)
#include "base/win_util.h"
#endif
#if defined(OS_MACOSX)
#include "chrome/app/breakpad_mac.h"
#endif
#include "chrome/app/scoped_ole_initializer.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_counters.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/main_function_params.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/common/sandbox_init_wrapper.h"
#if defined(OS_WIN)
#include "sandbox/src/sandbox.h"
#include "tools/memory_watcher/memory_watcher.h"
#endif
#if defined(OS_MACOSX)
#include "third_party/WebKit/WebKit/mac/WebCoreSupport/WebSystemInterface.h"
#endif

extern int BrowserMain(const MainFunctionParams&);
extern int RendererMain(const MainFunctionParams&);
extern int PluginMain(const MainFunctionParams&);
extern int WorkerMain(const MainFunctionParams&);

#if defined(OS_WIN)
// TODO(erikkay): isn't this already defined somewhere?
#define DLLEXPORT __declspec(dllexport)

// We use extern C for the prototype DLLEXPORT to avoid C++ name mangling.
extern "C" {
DLLEXPORT int __cdecl ChromeMain(HINSTANCE instance,
                                 sandbox::SandboxInterfaceInfo* sandbox_info,
                                 TCHAR* command_line);
}
#elif defined(OS_POSIX)
extern "C" {
int ChromeMain(int argc, const char** argv);
}
#endif

namespace {

#if defined(OS_WIN)
const wchar_t kProfilingDll[] = L"memory_watcher.dll";

// Load the memory profiling DLL.  All it needs to be activated
// is to be loaded.  Return true on success, false otherwise.
bool LoadMemoryProfiler() {
  HMODULE prof_module = LoadLibrary(kProfilingDll);
  return prof_module != NULL;
}

CAppModule _Module;

#pragma optimize("", off)
// Handlers for invalid parameter and pure call. They generate a breakpoint to
// tell breakpad that it needs to dump the process.
void InvalidParameter(const wchar_t* expression, const wchar_t* function,
                      const wchar_t* file, unsigned int line,
                      uintptr_t reserved) {
  __debugbreak();
}

void PureCall() {
  __debugbreak();
}

int OnNoMemory(size_t memory_size) {
  __debugbreak();
  // Return memory_size so it is not optimized out. Make sure the return value
  // is at least 1 so malloc/new is retried, especially useful when under a
  // debugger.
  return memory_size ? static_cast<int>(memory_size) : 1;
}

// Handlers to silently dump the current process when there is an assert in
// chrome.
void ChromeAssert(const std::string& str) {
  // Get the breakpad pointer from chrome.exe
  typedef void (__stdcall *DumpProcessFunction)();
  DumpProcessFunction DumpProcess = reinterpret_cast<DumpProcessFunction>(
      ::GetProcAddress(::GetModuleHandle(L"chrome.exe"), "DumpProcess"));
  if (DumpProcess)
    DumpProcess();
}

#pragma optimize("", on)

// Early versions of Chrome incorrectly registered a chromehtml: URL handler.
// Later versions fixed the registration but in some cases (e.g. Vista and non-
// admin installs) the fix could not be applied.  This prevents Chrome to be
// launched with the incorrect format.
// CORRECT: <broser.exe> -- "chromehtml:<url>"
// INVALID: <broser.exe> "chromehtml:<url>"
bool IncorrectChromeHtmlArguments(const std::wstring& command_line) {
  const wchar_t kChromeHtml[] = L"-- \"chromehtml:";
  const wchar_t kOffset = 5;  // Where chromehtml: starts in above
  std::wstring command_line_lower = command_line;

  // We are only searching for ASCII characters so this is OK.
  StringToLowerASCII(&command_line_lower);

  std::wstring::size_type pos = command_line_lower.find(
      kChromeHtml + kOffset);

  if (pos == std::wstring::npos)
    return false;

  // The browser is being launched with chromehtml: somewhere on the command
  // line.  We will not launch unless it's preceded by the -- switch terminator.
  if (pos >= kOffset) {
    if (equal(kChromeHtml, kChromeHtml + arraysize(kChromeHtml) - 1,
        command_line_lower.begin() + pos - kOffset)) {
      return false;
    }
  }

  return true;
}

#endif  // OS_WIN

#if defined(OS_LINUX)
static void GtkFatalLogHandler(const gchar* log_domain,
                               GLogLevelFlags log_level,
                               const gchar* message,
                               gpointer userdata) {
  if (!log_domain)
    log_domain = "<all>";
  if (!message)
    message = "<no message>";

  NOTREACHED() << "GTK: (" << log_domain << "): " << message;
}
#endif

// Register the invalid param handler and pure call handler to be able to
// notify breakpad when it happens.
void RegisterInvalidParamHandler() {
#if defined(OS_WIN)
  _set_invalid_parameter_handler(InvalidParameter);
  _set_purecall_handler(PureCall);
  // Gather allocation failure.
  _set_new_handler(&OnNoMemory);
  // Make sure malloc() calls the new handler too.
  _set_new_mode(1);
#endif
}

void SetupCRT(const CommandLine& parsed_command_line) {
#if defined(OS_WIN)
#ifdef _CRTDBG_MAP_ALLOC
  _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
  _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
#else
  if (!parsed_command_line.HasSwitch(switches::kDisableBreakpad)) {
    _CrtSetReportMode(_CRT_ASSERT, 0);
  }
#endif

  // Enable the low fragmentation heap for the CRT heap. The heap is not changed
  // if the process is run under the debugger is enabled or if certain gflags
  // are set.
  bool use_lfh = false;
  if (parsed_command_line.HasSwitch(switches::kUseLowFragHeapCrt))
    use_lfh = parsed_command_line.GetSwitchValue(switches::kUseLowFragHeapCrt)
        != L"false";
  if (use_lfh) {
    void* crt_heap = reinterpret_cast<void*>(_get_heap_handle());
    ULONG enable_lfh = 2;
    HeapSetInformation(crt_heap, HeapCompatibilityInformation,
                       &enable_lfh, sizeof(enable_lfh));
  }
#endif
}

// Enable the heap profiler if the appropriate command-line switch is
// present, bailing out of the app we can't.
void EnableHeapProfiler(const CommandLine& parsed_command_line) {
#if defined(OS_WIN)
  if (parsed_command_line.HasSwitch(switches::kMemoryProfiling))
    if (!LoadMemoryProfiler())
      exit(-1);
#endif
}

void CommonSubprocessInit() {
  // Initialize ResourceBundle which handles files loaded from external
  // sources.  The language should have been passed in to us from the
  // browser process as a command line flag.
  ResourceBundle::InitSharedInstance(std::wstring());

#if defined(OS_WIN)
  // HACK: Let Windows know that we have started.  This is needed to suppress
  // the IDC_APPSTARTING cursor from being displayed for a prolonged period
  // while a subprocess is starting.
  PostThreadMessage(GetCurrentThreadId(), WM_NULL, 0, 0);
  MSG msg;
  PeekMessage(&msg, NULL, 0, 0, PM_REMOVE);
#endif
}

}  // namespace

#if defined(OS_WIN)
DLLEXPORT int __cdecl ChromeMain(HINSTANCE instance,
                                 sandbox::SandboxInterfaceInfo* sandbox_info,
                                 TCHAR* command_line) {
#elif defined(OS_POSIX)
int ChromeMain(int argc, const char** argv) {
#endif

#if defined(OS_MACOSX)
  // If Breakpad is not present then turn off os crash dumps so we don't have
  // to wait eons for Apple's Crash Reporter to generate a dump.
  if (!IsCrashReporterEnabled()) {
    DebugUtil::DisableOSCrashDumps();
  }
#endif
  RegisterInvalidParamHandler();

  // The exit manager is in charge of calling the dtors of singleton objects.
  base::AtExitManager exit_manager;

  // We need this pool for all the objects created before we get to the
  // event loop, but we don't want to leave them hanging around until the
  // app quits. Each "main" needs to flush this pool right before it goes into
  // its main event loop to get rid of the cruft.
  base::ScopedNSAutoreleasePool autorelease_pool;

  // Initialize the command line.
#if defined(OS_WIN)
  CommandLine::Init(0, NULL);
#else
  CommandLine::Init(argc, argv);
#endif
  const CommandLine& parsed_command_line = *CommandLine::ForCurrentProcess();

#if defined(OS_WIN)
   // Must do this before any other usage of command line!
  if (::IncorrectChromeHtmlArguments(parsed_command_line.command_line_string()))
    return 1;
#endif

  int browser_pid;
  std::wstring process_type =
    parsed_command_line.GetSwitchValue(switches::kProcessType);
  if (process_type.empty()) {
    browser_pid = base::GetCurrentProcId();
  } else {
    std::wstring channel_name =
      parsed_command_line.GetSwitchValue(switches::kProcessChannelID);

    browser_pid = StringToInt(WideToASCII(channel_name));
    DCHECK(browser_pid != 0);
  }
  SetupCRT(parsed_command_line);

  // Initialize the Chrome path provider.
  chrome::RegisterPathProvider();

  // Initialize the Stats Counters table.  With this initialized,
  // the StatsViewer can be utilized to read counters outside of
  // Chrome.  These lines can be commented out to effectively turn
  // counters 'off'.  The table is created and exists for the life
  // of the process.  It is not cleaned up.
  // TODO(port): we probably need to shut this down correctly to avoid
  // leaking shared memory regions on posix platforms.
  if (parsed_command_line.HasSwitch(switches::kEnableStatsTable)) {
    std::string statsfile =
        StringPrintf("%s-%d", chrome::kStatsFilename, browser_pid);
    StatsTable *stats_table = new StatsTable(statsfile,
        chrome::kStatsMaxThreads, chrome::kStatsMaxCounters);
    StatsTable::set_current(stats_table);
  }

  StatsScope<StatsCounterTimer>
      startup_timer(chrome::Counters::chrome_main());

  // Enable the heap profiler as early as possible!
  EnableHeapProfiler(parsed_command_line);

  // Enable Message Loop related state asap.
  if (parsed_command_line.HasSwitch(switches::kMessageLoopHistogrammer))
    MessageLoop::EnableHistogrammer(true);

  // Checks if the sandbox is enabled in this process and initializes it if this
  // is the case. The crash handler depends on this so it has to be done before
  // its initialization.
  SandboxInitWrapper sandbox_wrapper;
#if defined(OS_WIN)
  sandbox_wrapper.SetServices(sandbox_info);
#endif
  sandbox_wrapper.InitializeSandbox(parsed_command_line, process_type);

#if defined(OS_WIN)
  _Module.Init(NULL, instance);
#endif

  // Notice a user data directory override if any
  const std::wstring user_data_dir =
      parsed_command_line.GetSwitchValue(switches::kUserDataDir);
  if (!user_data_dir.empty())
    PathService::Override(chrome::DIR_USER_DATA, user_data_dir);

  bool single_process =
#if defined (GOOGLE_CHROME_BUILD)
    // This is an unsupported and not fully tested mode, so don't enable it for
    // official Chrome builds.
    false;
#else
    parsed_command_line.HasSwitch(switches::kSingleProcess);
#endif
  if (single_process)
    RenderProcessHost::set_run_renderer_in_process(true);
#if defined(OS_MACOSX)
  // TODO(port-mac): This is from renderer_main_platform_delegate.cc.
  // shess tried to refactor things appropriately, but it sprawled out
  // of control because different platforms needed different styles of
  // initialization.  Try again once we understand the process
  // architecture needed and where it should live.
  if (single_process)
    InitWebCoreSystemInterface();
#endif

  bool icu_result = icu_util::Initialize();
  CHECK(icu_result);

  logging::OldFileDeletionState file_state =
      logging::APPEND_TO_OLD_LOG_FILE;
  if (process_type.empty()) {
    file_state = logging::DELETE_OLD_LOG_FILE;
  }
  logging::InitChromeLogging(parsed_command_line, file_state);

#ifdef NDEBUG
  if (parsed_command_line.HasSwitch(switches::kSilentDumpOnDCHECK) &&
      parsed_command_line.HasSwitch(switches::kEnableDCHECK)) {
#if defined(OS_WIN)
    logging::SetLogReportHandler(ChromeAssert);
#endif
  }
#endif  // NDEBUG

  if (!process_type.empty())
    CommonSubprocessInit();

  startup_timer.Stop();  // End of Startup Time Measurement.

  MainFunctionParams main_params(parsed_command_line, sandbox_wrapper,
                                 &autorelease_pool);

  // TODO(port): turn on these main() functions as they've been de-winified.
  int rv = -1;
  if (process_type == switches::kRendererProcess) {
    rv = RendererMain(main_params);
  } else if (process_type == switches::kPluginProcess) {
#if defined(OS_WIN)
    rv = PluginMain(main_params);
#endif
  } else if (process_type == switches::kWorkerProcess) {
#if defined(OS_WIN)
    rv = WorkerMain(main_params);
#endif
  } else if (process_type.empty()) {
#if defined(OS_LINUX)
    // gtk_init() can change |argc| and |argv|, but nobody else uses them.
    gtk_init(&argc, const_cast<char***>(&argv));
    // Register GTK assertions to go through our logging system.
    g_log_set_handler(NULL,  // All logging domains.
                      static_cast<GLogLevelFlags>(G_LOG_FLAG_RECURSION |
                                                  G_LOG_FLAG_FATAL |
                                                  G_LOG_LEVEL_ERROR |
                                                  G_LOG_LEVEL_CRITICAL |
                                                  G_LOG_LEVEL_WARNING),
                      GtkFatalLogHandler,
                      NULL);
#endif

    ScopedOleInitializer ole_initializer;
    rv = BrowserMain(main_params);
  } else {
    NOTREACHED() << "Unknown process type";
  }

  if (!process_type.empty()) {
    ResourceBundle::CleanupSharedInstance();
  }

#if defined(OS_WIN)
#ifdef _CRTDBG_MAP_ALLOC
  _CrtDumpMemoryLeaks();
#endif  // _CRTDBG_MAP_ALLOC

  _Module.Term();
#endif

  logging::CleanupChromeLogging();

  return rv;
}
