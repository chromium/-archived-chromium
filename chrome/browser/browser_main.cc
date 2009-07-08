// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#include <algorithm>

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/command_line.h"
#include "base/field_trial.h"
#include "base/file_util.h"
#include "base/histogram.h"
#include "base/lazy_instance.h"
#include "base/scoped_nsautorelease_pool.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "base/system_monitor.h"
#include "base/time.h"
#include "base/tracked_objects.h"
#include "base/values.h"
#include "chrome/browser/browser_main_win.h"
#include "chrome/browser/browser_init.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_prefs.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_impl.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/dom_ui/chrome_url_data_manager.h"
#include "chrome/browser/extensions/extension_protocols.h"
#include "chrome/browser/first_run.h"
#include "chrome/browser/metrics/metrics_service.h"
#include "chrome/browser/net/dns_global.h"
#include "chrome/browser/net/sdch_dictionary_fetcher.h"
#include "chrome/browser/plugin_service.h"
#include "chrome/browser/process_singleton.h"
#include "chrome/browser/profile_manager.h"
#include "chrome/browser/renderer_host/resource_dispatcher_host.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/user_data_manager.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/histogram_synchronizer.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/main_function_params.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/result_codes.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/master_preferences.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/net_resources.h"
#include "net/base/cookie_monster.h"
#include "net/base/net_module.h"
#include "net/http/http_network_session.h"

#if defined(OS_POSIX)
// TODO(port): get rid of this include. It's used just to provide declarations
// and stub definitions for classes we encouter during the porting effort.
#include "chrome/common/temp_scaffolding_stubs.h"
#include <errno.h>
#include <signal.h>
#include <sys/resource.h>
#endif

#if defined(OS_LINUX)
#include "chrome/app/breakpad_linux.h"
#endif

// TODO(port): several win-only methods have been pulled out of this, but
// BrowserMain() as a whole needs to be broken apart so that it's usable by
// other platforms. For now, it's just a stub. This is a serious work in
// progress and should not be taken as an indication of a real refactoring.

#if defined(OS_WIN)
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>

#include "app/win_util.h"
#include "base/registry.h"
#include "base/win_util.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_trial.h"
#include "chrome/browser/jankometer.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/rlz/rlz.h"
#include "chrome/browser/views/user_data_dir_dialog.h"
#include "chrome/common/env_vars.h"
#include "chrome/installer/util/helper.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/shell_util.h"
#include "chrome/installer/util/version.h"
#include "net/base/net_util.h"
#include "net/base/sdch_manager.h"
#include "net/base/winsock_init.h"
#include "net/http/http_network_layer.h"
#include "printing/printed_document.h"
#include "sandbox/src/sandbox.h"
#include "views/widget/accelerator_handler.h"
#endif  // defined(OS_WIN)

#if defined(TOOLKIT_GTK)
#include "chrome/common/gtk_util.h"
#elif defined(TOOLKIT_VIEWS)
#include "chrome/browser/views/chrome_views_delegate.h"
#endif

namespace Platform {

void WillInitializeMainMessageLoop(const CommandLine & command_line);
void WillTerminate();

#if defined(OS_WIN) || defined(OS_LINUX)
// Perform any platform-specific work that needs to be done before the main
// message loop is created and initialized.
void WillInitializeMainMessageLoop(const CommandLine & command_line) {
}

// Perform platform-specific work that needs to be done after the main event
// loop has ended.
void WillTerminate() {
}
#endif

}  // namespace Platform

namespace {

// This function provides some ways to test crash and assertion handling
// behavior of the program.
void HandleErrorTestParameters(const CommandLine& command_line) {
  // This parameter causes an assertion.
  if (command_line.HasSwitch(switches::kBrowserAssertTest)) {
    DCHECK(false);
  }

  // This parameter causes a null pointer crash (crash reporter trigger).
  if (command_line.HasSwitch(switches::kBrowserCrashTest)) {
    int* bad_pointer = NULL;
    *bad_pointer = 0;
  }
}

// The net module doesn't have access to this HTML or the strings that need to
// be localized.  The Chrome locale will never change while we're running, so
// it's safe to have a static string that we always return a pointer into.
// This allows us to have the ResourceProvider return a pointer into the actual
// resource (via a StringPiece), instead of always copying resources.
struct LazyDirectoryListerCacher {
  LazyDirectoryListerCacher() {
    DictionaryValue value;
    value.SetString(L"header",
                    l10n_util::GetString(IDS_DIRECTORY_LISTING_HEADER));
    value.SetString(L"parentDirText",
                    l10n_util::GetString(IDS_DIRECTORY_LISTING_PARENT));
    value.SetString(L"headerName",
                    l10n_util::GetString(IDS_DIRECTORY_LISTING_NAME));
    value.SetString(L"headerSize",
                    l10n_util::GetString(IDS_DIRECTORY_LISTING_SIZE));
    value.SetString(L"headerDateModified",
                    l10n_util::GetString(IDS_DIRECTORY_LISTING_DATE_MODIFIED));
    html_data = jstemplate_builder::GetTemplateHtml(
        ResourceBundle::GetSharedInstance().GetRawDataResource(
            IDR_DIR_HEADER_HTML),
        &value,
        "t");
  }

  std::string html_data;
};

base::LazyInstance<LazyDirectoryListerCacher> lazy_dir_lister(
    base::LINKER_INITIALIZED);

// This is called indirectly by the network layer to access resources.
StringPiece NetResourceProvider(int key) {
  if (IDR_DIR_HEADER_HTML == key)
    return StringPiece(lazy_dir_lister.Pointer()->html_data);

  return ResourceBundle::GetSharedInstance().GetRawDataResource(key);
}

void RunUIMessageLoop(BrowserProcess* browser_process) {
#if defined(OS_WIN)
  MessageLoopForUI::current()->Run(browser_process->accelerator_handler());
#elif defined(OS_POSIX)
  MessageLoopForUI::current()->Run();
#endif
}

#if defined(OS_POSIX)
// See comment below, where sigaction is called.
void SIGCHLDHandler(int signal) {
}

// Sets the file descriptor soft limit to |max_descriptors| or the OS hard
// limit, whichever is lower.
void SetFileDescriptorLimit(unsigned int max_descriptors) {
  struct rlimit limits;
  if (getrlimit(RLIMIT_NOFILE, &limits) == 0) {
    unsigned int new_limit = max_descriptors;
    if (limits.rlim_max > 0 && limits.rlim_max < max_descriptors) {
      new_limit = limits.rlim_max;
    }
    limits.rlim_cur = new_limit;
    if (setrlimit(RLIMIT_NOFILE, &limits) != 0) {
      LOG(INFO) << "Failed to set file descriptor limit: " << strerror(errno);
    }
  } else {
    LOG(INFO) << "Failed to get file descriptor limit: " << strerror(errno);
  }
}
#endif

#if defined(OS_WIN)
void AddFirstRunNewTabs(BrowserInit* browser_init,
                        const std::vector<std::wstring>& new_tabs) {
  std::vector<std::wstring>::const_iterator it = new_tabs.begin();
  while (it != new_tabs.end()) {
    GURL url(*it);
    if (url.is_valid())
      browser_init->AddFirstRunTab(url);
    ++it;
  }
}
#else
// TODO(cpu): implement first run experience for other platforms.
void AddFirstRunNewTabs(BrowserInit* browser_init,
                        const std::vector<std::wstring>& new_tabs) {
}
#endif

}  // namespace

// Main routine for running as the Browser process.
int BrowserMain(const MainFunctionParams& parameters) {
  const CommandLine& parsed_command_line = parameters.command_line_;
  base::ScopedNSAutoreleasePool* pool = parameters.autorelease_pool_;

#if defined(OS_LINUX)
  // Needs to be called after we have chrome::DIR_USER_DATA.
  InitCrashReporter();
#endif

  // WARNING: If we get a WM_ENDSESSION objects created on the stack here
  // are NOT deleted. If you need something to run during WM_ENDSESSION add it
  // to browser_shutdown::Shutdown or BrowserProcess::EndSession.

  // TODO(beng, brettw): someday, break this out into sub functions with well
  //                     defined roles (e.g. pre/post-profile startup, etc).

#ifdef TRACK_ALL_TASK_OBJECTS
  // Start tracking the creation and deletion of Task instance.
  // This construction MUST be done before main_message_loop, so that it is
  // destroyed after the main_message_loop.
  tracked_objects::AutoTracking tracking_objects;
#endif

#if defined(OS_POSIX)
  // We need to accept SIGCHLD, even though our handler is a no-op because
  // otherwise we cannot wait on children. (According to POSIX 2001.)
  struct sigaction action;
  memset(&action, 0, sizeof(action));
  action.sa_handler = SIGCHLDHandler;
  CHECK(sigaction(SIGCHLD, &action, NULL) == 0);

  const std::wstring fd_limit_string =
      parsed_command_line.GetSwitchValue(switches::kFileDescriptorLimit);
  int fd_limit = 0;
  if (!fd_limit_string.empty()) {
    StringToInt(WideToUTF16Hack(fd_limit_string), &fd_limit);
  }
#if defined(OS_MACOSX)
  // We use quite a few file descriptors for our IPC, and the default limit on
  // the Mac is low (256), so bump it up if there is no explicit override.
  if (fd_limit == 0) {
    fd_limit = 1024;
  }
#endif  // OS_MACOSX
  if (fd_limit > 0) {
    SetFileDescriptorLimit(fd_limit);
  }
#endif  // OS_POSIX

  // Do platform-specific things (such as finishing initializing Cocoa)
  // prior to instantiating the message loop. This could be turned into a
  // broadcast notification.
  Platform::WillInitializeMainMessageLoop(parsed_command_line);

  MessageLoop main_message_loop(MessageLoop::TYPE_UI);

  // Initialize the SystemMonitor
  base::SystemMonitor::Start();
#if defined(OS_WIN)
  // We want to monitor system power state to adjust our high resolution
  // timer settings. But it's necessary only on Windows.
  base::Time::StartSystemMonitorObserver();
#endif  // defined(OS_WIN)

  // Initialize statistical testing infrastructure.
  FieldTrialList field_trial;

  std::wstring app_name = chrome::kBrowserAppName;
  std::string thread_name_string = WideToASCII(app_name + L"_BrowserMain");

  const char* thread_name = thread_name_string.c_str();
  PlatformThread::SetName(thread_name);
  main_message_loop.set_thread_name(thread_name);
  bool already_running = Upgrade::IsBrowserAlreadyRunning();

  FilePath user_data_dir;
  PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  ProcessSingleton process_singleton(user_data_dir);

  bool is_first_run = FirstRun::IsChromeFirstRun() ||
      parsed_command_line.HasSwitch(switches::kFirstRun);
  bool first_run_ui_bypass = false;

  scoped_ptr<BrowserProcess> browser_process;
  if (parsed_command_line.HasSwitch(switches::kImport)) {
    // We use different BrowserProcess when importing so no GoogleURLTracker is
    // instantiated (as it makes a URLRequest and we don't have an IO thread,
    // see bug #1292702).
    browser_process.reset(new FirstRunBrowserProcess(parsed_command_line));
    is_first_run = false;
  } else {
    browser_process.reset(new BrowserProcessImpl(parsed_command_line));
  }

  // BrowserProcessImpl's constructor should set g_browser_process.
  DCHECK(g_browser_process);

#if defined(OS_WIN)
  // IMPORTANT: This piece of code needs to run as early as possible in the
  // process because it will initialize the sandbox broker, which requires the
  // process to swap its window station. During this time all the UI will be
  // broken. This has to run before threads and windows are created.
  sandbox::BrokerServices* broker_services =
      parameters.sandbox_info_.BrokerServices();
  if (broker_services) {
    browser_process->InitBrokerServices(broker_services);
    if (!parsed_command_line.HasSwitch(switches::kNoSandbox)) {
      bool use_winsta = !parsed_command_line.HasSwitch(
                            switches::kDisableAltWinstation);
      // Precreate the desktop and window station used by the renderers.
      sandbox::TargetPolicy* policy = broker_services->CreatePolicy();
      sandbox::ResultCode result = policy->CreateAlternateDesktop(use_winsta);
      CHECK(sandbox::SBOX_ERROR_FAILED_TO_SWITCH_BACK_WINSTATION != result);
      policy->Release();
    }
  }
#endif

  std::wstring local_state_path;
  PathService::Get(chrome::FILE_LOCAL_STATE, &local_state_path);
  bool local_state_file_exists = file_util::PathExists(local_state_path);

  // Load local state.  This includes the application locale so we know which
  // locale dll to load.
  PrefService* local_state = browser_process->local_state();
  DCHECK(local_state);

  // Initialize ResourceBundle which handles files loaded from external
  // sources. This has to be done before uninstall code path and before prefs
  // are registered.
  local_state->RegisterStringPref(prefs::kApplicationLocale, L"");
  local_state->RegisterBooleanPref(prefs::kMetricsReportingEnabled, false);

#if defined(TOOLKIT_GTK)
  // It is important for this to happen before the first run dialog, as it
  // styles the dialog as well.
  gtk_util::InitRCStyles();
#elif defined(TOOLKIT_VIEWS)
  // The delegate needs to be set before any UI is created so that windows
  // display the correct icon.
  if (!views::ViewsDelegate::views_delegate)
    views::ViewsDelegate::views_delegate = new ChromeViewsDelegate;
#endif


#if defined(OS_POSIX)
  // On Mac OS X / Linux we display the first run dialog as early as possible,
  // so we can get the stats enabled.
  // TODO(port):
  // We check the kNoFirstRun command line switch explicitly here since the
  // early placement of this block happens before that's factored into
  // first_run_ui_bypass, we probably want to move that block up
  // and remove the explicit check from here in the long run.
  if (is_first_run && !first_run_ui_bypass &&
      !parsed_command_line.HasSwitch(switches::kNoFirstRun)) {
    // Dummy value, we don't need the profile for the OS X version of this
    // method at present.
    Profile* profile = NULL;
    OpenFirstRunDialog(profile, &process_singleton);

#if defined(GOOGLE_CHROME_BUILD)
    // If user cancelled the first run dialog box, the first run sentinel file
    // didn't get created and we should exit Chrome.
    if (FirstRun::IsChromeFirstRun())
      return ResultCodes::NORMAL_EXIT;
#endif
  }
#endif  // OS_POSIX

  // During first run we read the google_update registry key to find what
  // language the user selected when downloading the installer. This
  // becomes our default language in the prefs.
  if (is_first_run) {
    std::wstring install_lang;
    if (GoogleUpdateSettings::GetLanguage(&install_lang))
      local_state->SetString(prefs::kApplicationLocale, install_lang);
    if (GoogleUpdateSettings::GetCollectStatsConsent())
      local_state->SetBoolean(prefs::kMetricsReportingEnabled, true);
  }

  // If the local state file for the current profile doesn't exist and the
  // parent profile command line flag is present, then we should inherit some
  // local state from the parent profile.
  // Checking that the local state file for the current profile doesn't exist
  // is the most robust way to determine whether we need to inherit or not
  // since the parent profile command line flag can be present even when the
  // current profile is not a new one, and in that case we do not want to
  // inherit and reset the user's setting.
  if (!local_state_file_exists &&
      parsed_command_line.HasSwitch(switches::kParentProfile)) {
    FilePath parent_profile = FilePath::FromWStringHack(
        parsed_command_line.GetSwitchValue(switches::kParentProfile));
    PrefService parent_local_state(parent_profile,
                                   g_browser_process->file_thread());
    parent_local_state.RegisterStringPref(prefs::kApplicationLocale,
                                          std::wstring());
    // Right now, we only inherit the locale setting from the parent profile.
    local_state->SetString(
        prefs::kApplicationLocale,
        parent_local_state.GetString(prefs::kApplicationLocale));
  }

  // If we're running tests (ui_task is non-null), then the ResourceBundle
  // has already been initialized.
  if (!parameters.ui_task) {
    ResourceBundle::InitSharedInstance(
        local_state->GetString(prefs::kApplicationLocale));
    // We only load the theme dll in the browser process.
    ResourceBundle::GetSharedInstance().LoadThemeResources();
  }

#if defined(OS_WIN)
  // This is experimental code. See first_run_win.cc for more info.
  if (parsed_command_line.HasSwitch(switches::kTryChromeAgain)) {
    Upgrade::TryResult answer = Upgrade::ShowTryChromeDialog();
    if (answer == Upgrade::TD_NOT_NOW)
      return ResultCodes::NORMAL_EXIT_EXP1;
    if (answer == Upgrade::TD_UNINSTALL_CHROME)
      return ResultCodes::NORMAL_EXIT_EXP2;
  }
#endif  // OS_WIN

  BrowserInit browser_init;

  if (is_first_run) {
    // On first run, we  need to process the master preferences before the
    // browser's profile_manager object is created, but after ResourceBundle
    // is initialized.
    std::vector<std::wstring> first_run_tabs;
    first_run_ui_bypass =
        !FirstRun::ProcessMasterPreferences(user_data_dir, FilePath(), NULL,
                                            &first_run_tabs);
    // The master prefs might specify a set of urls to display.
    if (first_run_tabs.size())
      AddFirstRunNewTabs(&browser_init, first_run_tabs);

    // If we are running in App mode, we do not want to show the importer
    // (first run) UI.
    if (!first_run_ui_bypass &&
        (parsed_command_line.HasSwitch(switches::kApp) ||
         parsed_command_line.HasSwitch(switches::kNoFirstRun))) {
      first_run_ui_bypass = true;
    }
  }

  if (!parsed_command_line.HasSwitch(switches::kNoErrorDialogs)) {
    // Display a warning if the user is running windows 2000.
    // TODO(port): We should probably change this to a "check for minimum
    // requirements" function, implemented by each platform.
    CheckForWin2000();
  }

  if (parsed_command_line.HasSwitch(switches::kEnableFileCookies)) {
    // Enable cookie storage for file:// URLs.  Must do this before the first
    // Profile (and therefore the first CookieMonster) is created.
    net::CookieMonster::EnableFileScheme();
  }

  // Initialize histogram statistics gathering system.
  StatisticsRecorder statistics;

  // Initialize histogram synchronizer system. This is a singleton and is used
  // for posting tasks via NewRunnableMethod. Its deleted when it goes out of
  // scope. Even though NewRunnableMethod does AddRef and Release, the object
  // will not be deleted after the Task is executed.
  scoped_refptr<HistogramSynchronizer> histogram_synchronizer =
      new HistogramSynchronizer();

  // Initialize the shared instance of user data manager.
  scoped_ptr<UserDataManager> user_data_manager(UserDataManager::Create());

  // Try to create/load the profile.
  ProfileManager* profile_manager = browser_process->profile_manager();
  Profile* profile = profile_manager->GetDefaultProfile(user_data_dir);
  if (!profile) {
    // Ideally, we should be able to run w/o access to disk.  For now, we
    // prompt the user to pick a different user-data-dir and restart chrome
    // with the new dir.
    // http://code.google.com/p/chromium/issues/detail?id=11510
#if defined(OS_WIN)
    user_data_dir = FilePath::FromWStringHack(
        UserDataDirDialog::RunUserDataDirDialog(user_data_dir.ToWStringHack()));
#elif defined(OS_LINUX)
    // TODO(port): fix this.
    user_data_dir = FilePath("/tmp");
#endif
#if defined(OS_WIN) || defined(OS_LINUX)
    if (!parameters.ui_task && browser_shutdown::delete_resources_on_shutdown) {
      // Only delete the resources if we're not running tests. If we're running
      // tests the resources need to be reused as many places in the UI cache
      // SkBitmaps from the ResourceBundle.
      ResourceBundle::CleanupSharedInstance();
    }

    if (!user_data_dir.empty()) {
      // Because of the way CommandLine parses, it's sufficient to append a new
      // --user-data-dir switch.  The last flag of the same name wins.
      // TODO(tc): It would be nice to remove the flag we don't want, but that
      // sounds risky if we parse differently than CommandLineToArgvW.
      CommandLine new_command_line = parsed_command_line;
      new_command_line.AppendSwitchWithValue(switches::kUserDataDir,
                                             user_data_dir.ToWStringHack());
      base::LaunchApp(new_command_line, false, false, NULL);
    }

    return ResultCodes::NORMAL_EXIT;
#endif  // defined(OS_WIN) || defined(OS_LINUX)
  }

  PrefService* user_prefs = profile->GetPrefs();
  DCHECK(user_prefs);

  // Now that local state and user prefs have been loaded, make the two pref
  // services aware of all our preferences.
  browser::RegisterAllPrefs(user_prefs, local_state);

  // Now that all preferences have been registered, set the install date
  // for the uninstall metrics if this is our first run. This only actually
  // gets used if the user has metrics reporting enabled at uninstall time.
  int64 install_date =
      local_state->GetInt64(prefs::kUninstallMetricsInstallDate);
  if (install_date == 0) {
    local_state->SetInt64(prefs::kUninstallMetricsInstallDate,
                          base::Time::Now().ToTimeT());
  }

  // Record last shutdown time into a histogram.
  browser_shutdown::ReadLastShutdownInfo();

  // If the command line specifies 'uninstall' then we need to work here
  // unless we detect another chrome browser running.
  if (parsed_command_line.HasSwitch(switches::kUninstall))
    return DoUninstallTasks(already_running);

  if (parsed_command_line.HasSwitch(switches::kHideIcons) ||
      parsed_command_line.HasSwitch(switches::kShowIcons)) {
    return HandleIconsCommands(parsed_command_line);
  } else if (parsed_command_line.HasSwitch(switches::kMakeDefaultBrowser)) {
    if (ShellIntegration::SetAsDefaultBrowser()) {
      return ResultCodes::NORMAL_EXIT;
    } else {
      return ResultCodes::SHELL_INTEGRATION_FAILED;
    }
  }

  // Importing other browser settings is done in a browser-like process
  // that exits when this task has finished.
  if (parsed_command_line.HasSwitch(switches::kImport))
    return FirstRun::ImportNow(profile, parsed_command_line);

  // When another process is running, use it instead of starting us.
  if (process_singleton.NotifyOtherProcess())
    return ResultCodes::NORMAL_EXIT;

  // Do the tasks if chrome has been upgraded while it was last running.
  if (!already_running && DoUpgradeTasks(parsed_command_line)) {
    return ResultCodes::NORMAL_EXIT;
  }

  // Check if there is any machine level Chrome installed on the current
  // machine. If yes and the current Chrome process is user level, we do not
  // allow the user level Chrome to run. So we notify the user and uninstall
  // user level Chrome.
  // Note this check should only happen here, after all the checks above
  // (uninstall, resource bundle initialization, other chrome browser
  // processes etc).
  if (CheckMachineLevelInstall())
    return ResultCodes::MACHINE_LEVEL_INSTALL_EXISTS;

  process_singleton.Create();

  // TODO(port): This block of code should probably be used on all platforms!
  // On Mac OS X / Linux we display this dialog before setting the value of
  // kMetricsReportingEnabled, so we display this dialog much earlier.
  // On Windows a download is tagged with stats enabled/disabled so the UI
  // can be displayed later in the startup process.
#if !defined(OS_POSIX)
  // Show the First Run UI if this is the first time Chrome has been run on
  // this computer, or we're being compelled to do so by a command line flag.
  // Note that this be done _after_ the PrefService is initialized and all
  // preferences are registered, since some of the code that the importer
  // touches reads preferences.
  if (is_first_run && !first_run_ui_bypass) {
    if (!OpenFirstRunDialog(profile, &process_singleton)) {
      // The user cancelled the first run dialog box, we should exit Chrome.
      return ResultCodes::NORMAL_EXIT;
    }
  }
#endif  // OS_POSIX

  // Sets things up so that if we crash from this point on, a dialog will
  // popup asking the user to restart chrome. It is done this late to avoid
  // testing against a bunch of special cases that are taken care early on.
  PrepareRestartOnCrashEnviroment(parsed_command_line);

#if defined(OS_WIN)
  // Initialize Winsock.
  net::EnsureWinsockInit();
#endif  // defined(OS_WIN)

  // Initialize and maintain DNS prefetcher module.
  chrome_browser_net::DnsPrefetcherInit dns_prefetch(user_prefs, local_state);

  scoped_refptr<FieldTrial> http_prioritization_trial =
      new FieldTrial("HttpPrioritization", 100);
  // Put 10% of people in the fallback experiment with the http prioritization
  // code disabled.
  const int holdback_group =
      http_prioritization_trial->AppendGroup("_no_http_prioritization", 10);
  if (http_prioritization_trial->group() == holdback_group) {
    ResourceDispatcherHost::DisableHttpPrioritization();
  }

#if defined(OS_WIN)
  // Init common control sex.
  INITCOMMONCONTROLSEX config;
  config.dwSize = sizeof(config);
  config.dwICC = ICC_WIN95_CLASSES;
  InitCommonControlsEx(&config);

  win_util::ScopedCOMInitializer com_initializer;

  int delay = 0;
  installer_util::GetDistributionPingDelay(FilePath(), delay);
  // Init the RLZ library. This just binds the dll and schedules a task on the
  // file thread to be run sometime later. If this is the first run we record
  // the installation event.
  RLZTracker::InitRlzDelayed(base::DIR_MODULE, is_first_run, delay);
#endif

  // Config the network module so it has access to resources.
  net::NetModule::SetResourceProvider(NetResourceProvider);

  // Register our global network handler for chrome:// and
  // chrome-extension:// URLs.
  RegisterURLRequestChromeJob();
  RegisterExtensionProtocols();

  // In unittest mode, this will do nothing.  In normal mode, this will create
  // the global GoogleURLTracker instance, which will promptly go to sleep for
  // five seconds (to avoid slowing startup), and wake up afterwards to see if
  // it should do anything else.  If we don't cause this creation now, it won't
  // happen until someone else asks for the tracker, at which point we may no
  // longer want to sleep for five seconds.
  //
  // A simpler way of doing all this would be to have some function which could
  // give the time elapsed since startup, and simply have the tracker check that
  // when asked to initialize itself, but this doesn't seem to exist.
  //
  // This can't be created in the BrowserProcessImpl constructor because it
  // needs to read prefs that get set after that runs.
  browser_process->google_url_tracker();

  // Have Chrome plugins write their data to the profile directory.
  PluginService::GetInstance()->SetChromePluginDataDir(profile->GetPath());

  // Prepare for memory caching of SDCH dictionaries.
  // Perform A/B test to measure global impact of SDCH support.
  // Set up a field trial to see what disabling SDCH does to latency of page
  // layout globally.
  FieldTrial::Probability kSDCH_DIVISOR = 100;
  FieldTrial::Probability kSDCH_PROBABILITY_PER_GROUP = 50;  // 50% probability.
  scoped_refptr<FieldTrial> sdch_trial =
      new FieldTrial("GlobalSdch", kSDCH_DIVISOR);

  bool need_to_init_sdch = true;
  std::string switch_domain("");
  if (parsed_command_line.HasSwitch(switches::kSdchFilter)) {
    switch_domain =
        WideToASCII(parsed_command_line.GetSwitchValue(switches::kSdchFilter));
  } else {
    sdch_trial->AppendGroup("_global_disable_sdch",
                            kSDCH_PROBABILITY_PER_GROUP);
    int sdch_enabled = sdch_trial->AppendGroup("_global_enable_sdch",
                                               kSDCH_PROBABILITY_PER_GROUP);
    need_to_init_sdch = (sdch_enabled == sdch_trial->group());
  }

  scoped_ptr<SdchManager> sdch_manager;  // Singleton database.
  if (need_to_init_sdch) {
    sdch_manager.reset(new SdchManager);
    sdch_manager->set_sdch_fetcher(new SdchDictionaryFetcher);
    // Use default of "" so that all domains are supported.
    sdch_manager->EnableSdchSupport(switch_domain);
  }

  MetricsService* metrics = NULL;
  if (!parsed_command_line.HasSwitch(switches::kDisableMetrics)) {
    bool enabled = local_state->GetBoolean(prefs::kMetricsReportingEnabled);
    bool record_only =
        parsed_command_line.HasSwitch(switches::kMetricsRecordingOnly);

#if !defined(GOOGLE_CHROME_BUILD)
    // Disable user metrics completely for non-Google Chrome builds.
    enabled = false;
#endif

    if (record_only) {
      local_state->transient()->SetBoolean(prefs::kMetricsReportingEnabled,
                                           false);
    }
    metrics = browser_process->metrics_service();
    DCHECK(metrics);

    // If we're testing then we don't care what the user preference is, we turn
    // on recording, but not reporting, otherwise tests fail.
    if (record_only) {
      metrics->StartRecordingOnly();
    } else {
      // If the user permits metrics reporting with the checkbox in the
      // prefs, we turn on recording.
      metrics->SetUserPermitsUpload(enabled);
      if (enabled)
        metrics->Start();
    }
  }
  InstallJankometer(parsed_command_line);

#if defined(OS_WIN) && !defined(GOOGLE_CHROME_BUILD)
  if (parsed_command_line.HasSwitch(switches::kDebugPrint)) {
    printing::PrintedDocument::set_debug_dump_path(
        parsed_command_line.GetSwitchValue(switches::kDebugPrint));
  }
#endif

  HandleErrorTestParameters(parsed_command_line);
  RecordBreakpadStatusUMA(metrics);
  // Start up the extensions service. This should happen before Start().
  profile->InitExtensions();
  // Start up the web resource service.  This starts loading data after a
  // short delay so as not to interfere with startup time.
  if (parsed_command_line.HasSwitch(switches::kWebResources))
    profile->InitWebResources();

  int result_code = ResultCodes::NORMAL_EXIT;
  if (parameters.ui_task) {
    // We are in test mode. Run one task and enter the main message loop.
    if (pool)
      pool->Recycle();
    MessageLoopForUI::current()->PostTask(FROM_HERE, parameters.ui_task);
    RunUIMessageLoop(browser_process.get());
  } else {
    // We are in regular browser boot sequence. Open initial stabs and enter
    // the main message loop.
    if (browser_init.Start(parsed_command_line, std::wstring(), profile,
                           &result_code)) {
      // Call Recycle() here as late as possible, before going into the loop
      // because Start() will add things to it while creating the main window.
      if (pool)
        pool->Recycle();
      RunUIMessageLoop(browser_process.get());
    }
  }

  Platform::WillTerminate();

  if (metrics)
    metrics->Stop();

  // browser_shutdown takes care of deleting browser_process, so we need to
  // release it.
  browser_process.release();
  browser_shutdown::Shutdown();

  return result_code;
}
