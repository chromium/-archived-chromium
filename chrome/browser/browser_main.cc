// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#include <algorithm>

#include "base/command_line.h"
#include "base/field_trial.h"
#include "base/file_util.h"
#include "base/histogram.h"
#include "base/lazy_instance.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "base/system_monitor.h"
#include "base/tracked_objects.h"
#include "base/values.h"
#include "chrome/app/result_codes.h"
#include "chrome/browser/browser_main_win.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/plugin_service.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/main_function_params.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"

#include "chromium_strings.h"
#include "generated_resources.h"

#if defined(OS_POSIX)
// TODO(port): get rid of this include. It's used just to provide declarations
// and stub definitions for classes we encouter during the porting effort.
#include "chrome/common/temp_scaffolding_stubs.h"
#endif

// TODO(port): several win-only methods have been pulled out of this, but
// BrowserMain() as a whole needs to be broken apart so that it's usable by
// other platforms. For now, it's just a stub. This is a serious work in
// progress and should not be taken as an indication of a real refactoring.

#if defined(OS_WIN)

#include <windows.h>
#include <shellapi.h>

#include "base/registry.h"
#include "base/win_util.h"
#include "chrome/browser/automation/automation_provider.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_init.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_prefs.h"
#include "chrome/browser/browser_process_impl.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/browser_trial.h"
#include "chrome/browser/dom_ui/chrome_url_data_manager.h"
#include "chrome/browser/extensions/extension_protocols.h"
#include "chrome/browser/first_run.h"
#include "chrome/browser/jankometer.h"
#include "chrome/browser/message_window.h"
#include "chrome/browser/metrics/metrics_service.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/net/dns_global.h"
#include "chrome/browser/net/sdch_dictionary_fetcher.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/browser/printing/print_job_manager.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/profile_manager.h"
#include "chrome/browser/rlz/rlz.h"
#include "chrome/browser/user_data_manager.h"
#include "chrome/browser/views/user_data_dir_dialog.h"
#include "chrome/common/env_vars.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/common/win_util.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/helper.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/shell_util.h"
#include "chrome/installer/util/version.h"
#include "chrome/views/accelerator_handler.h"
#include "net/base/net_module.h"
#include "net/base/net_util.h"
#include "net/base/sdch_manager.h"
#include "net/base/winsock_init.h"
#include "net/http/http_network_layer.h"
#include "sandbox/src/sandbox.h"

#include "net_resources.h"

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

#if defined(OS_WIN)
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
#endif

}  // namespace

// Main routine for running as the Browser process.
int BrowserMain(const MainFunctionParams& parameters) {
  const CommandLine& parsed_command_line = parameters.command_line_;

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

  // Do platform-specific things (such as finishing initializing Cocoa)
  // prior to instantiating the message loop. This could be turned into a
  // broadcast notification.
  Platform::WillInitializeMainMessageLoop(parsed_command_line);

  MessageLoop main_message_loop(MessageLoop::TYPE_UI);

  // Initialize the SystemMonitor
  base::SystemMonitor::Start();

  // Initialize statistical testing infrastructure.
  FieldTrialList field_trial;

  std::wstring app_name = chrome::kBrowserAppName;
  std::string thread_name_string = WideToASCII(app_name + L"_BrowserMain");

  const char* thread_name = thread_name_string.c_str();
  PlatformThread::SetName(thread_name);
  main_message_loop.set_thread_name(thread_name);
  bool already_running = Upgrade::IsBrowserAlreadyRunning();

  std::wstring user_data_dir;
  PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  MessageWindow message_window(user_data_dir);

  scoped_ptr<BrowserProcess> browser_process;
  if (parsed_command_line.HasSwitch(switches::kImport)) {
    // We use different BrowserProcess when importing so no GoogleURLTracker is
    // instantiated (as it makes a URLRequest and we don't have an IO thread,
    // see bug #1292702).
    browser_process.reset(new FirstRunBrowserProcess(parsed_command_line));
  } else {
    browser_process.reset(new BrowserProcessImpl(parsed_command_line));
  }

  // BrowserProcessImpl's constructor should set g_browser_process.
  DCHECK(g_browser_process);

  std::wstring local_state_path;
  PathService::Get(chrome::FILE_LOCAL_STATE, &local_state_path);
  bool local_state_file_exists = file_util::PathExists(local_state_path);

  // Load local state.  This includes the application locale so we know which
  // locale dll to load.
  PrefService* local_state = browser_process->local_state();
  DCHECK(local_state);

  bool is_first_run = FirstRun::IsChromeFirstRun() ||
      parsed_command_line.HasSwitch(switches::kFirstRun);
  bool first_run_ui_bypass = false;

  // Initialize ResourceBundle which handles files loaded from external
  // sources. This has to be done before uninstall code path and before prefs
  // are registered.
  local_state->RegisterStringPref(prefs::kApplicationLocale, L"");
  local_state->RegisterBooleanPref(prefs::kMetricsReportingEnabled, false);

  // During first run we read the google_update registry key to find what
  // language the user selected when downloading the installer. This
  // becomes our default language in the prefs.
  if (is_first_run) {
    std::wstring install_lang;
    if (GoogleUpdateSettings::GetLanguage(&install_lang))
      local_state->SetString(prefs::kApplicationLocale, install_lang);
    if (GoogleUpdateSettings::GetCollectStatsConsent())
      local_state->SetBoolean(prefs::kMetricsReportingEnabled, true);
    // On first run, we  need to process the master preferences before the
    // browser's profile_manager object is created.
    first_run_ui_bypass =
        !FirstRun::ProcessMasterPreferences(user_data_dir,
                                            std::wstring(), NULL);

    // If we are running in App mode, we do not want to show the importer
    // (first run) UI.
    if (!first_run_ui_bypass && parsed_command_line.HasSwitch(switches::kApp))
      first_run_ui_bypass = true;
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
    std::wstring parent_profile =
        parsed_command_line.GetSwitchValue(switches::kParentProfile);
    PrefService parent_local_state(parent_profile);
    parent_local_state.RegisterStringPref(prefs::kApplicationLocale,
                                          std::wstring());
    // Right now, we only inherit the locale setting from the parent profile.
    local_state->SetString(
        prefs::kApplicationLocale,
        parent_local_state.GetString(prefs::kApplicationLocale));
  }

#if defined(OS_WIN)
  ResourceBundle::InitSharedInstance(
      local_state->GetString(prefs::kApplicationLocale));
  // We only load the theme dll in the browser process.
  ResourceBundle::GetSharedInstance().LoadThemeResources();
#endif

  if (!parsed_command_line.HasSwitch(switches::kNoErrorDialogs)) {
    // Display a warning if the user is running windows 2000.
    // TODO(port). We should probably change this to a "check for minimum
    // requirements" function, implemented by each platform.
    CheckForWin2000();
  }

  // Initialize histogram statistics gathering system.
  StatisticsRecorder statistics;

  // Initialize the shared instance of user data manager.
  scoped_ptr<UserDataManager> user_data_manager(UserDataManager::Create());

  // Try to create/load the profile.
  ProfileManager* profile_manager = browser_process->profile_manager();
  Profile* profile = profile_manager->GetDefaultProfile(user_data_dir);
  if (!profile) {
#if defined(OS_WIN)
    user_data_dir = UserDataDirDialog::RunUserDataDirDialog(user_data_dir);
    // Flush the message loop which lets the UserDataDirDialog close.
    MessageLoop::current()->Run();

    ResourceBundle::CleanupSharedInstance();

    if (!user_data_dir.empty()) {
      // Because of the way CommandLine parses, it's sufficient to append a new
      // --user-data-dir switch.  The last flag of the same name wins.
      // TODO(tc): It would be nice to remove the flag we don't want, but that
      // sounds risky if we parse differently than CommandLineToArgvW.
      CommandLine new_command_line = parsed_command_line;
      new_command_line.AppendSwitchWithValue(switches::kUserDataDir,
                                             user_data_dir);
      base::LaunchApp(new_command_line, false, false, NULL);
    }

    return ResultCodes::NORMAL_EXIT;
#endif
  }

  PrefService* user_prefs = profile->GetPrefs();
  DCHECK(user_prefs);

  // Now that local state and user prefs have been loaded, make the two pref
  // services aware of all our preferences.
  browser::RegisterAllPrefs(user_prefs, local_state);

  // Record last shutdown time into a histogram.
  browser_shutdown::ReadLastShutdownInfo();

  // If the command line specifies 'uninstall' then we need to work here
  // unless we detect another chrome browser running.
  if (parsed_command_line.HasSwitch(switches::kUninstall)) {
    if (already_running) {
#if defined(OS_WIN)
      const std::wstring text = l10n_util::GetString(IDS_UNINSTALL_CLOSE_APP);
      const std::wstring caption = l10n_util::GetString(IDS_PRODUCT_NAME);
      win_util::MessageBox(NULL, text, caption,
                           MB_OK | MB_ICONWARNING | MB_TOPMOST);
#endif
      return ResultCodes::UNINSTALL_CHROME_ALIVE;
    } else {
      return DoUninstallTasks();
    }
  }

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
  if (message_window.NotifyOtherProcess())
    return ResultCodes::NORMAL_EXIT;

  message_window.HuntForZombieChromeProcesses();

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

  message_window.Create();

  // Show the First Run UI if this is the first time Chrome has been run on
  // this computer, or we're being compelled to do so by a command line flag.
  // Note that this be done _after_ the PrefService is initialized and all
  // preferences are registered, since some of the code that the importer
  // touches reads preferences.
  if (is_first_run && !first_run_ui_bypass) {
    // We need to avoid dispatching new tabs when we are doing the import
    // because that will lead to data corruption or a crash. Lock() does that.
    message_window.Lock();
    OpenFirstRunDialog(profile);
    message_window.Unlock();
  }

  // Sets things up so that if we crash from this point on, a dialog will
  // popup asking the user to restart chrome. It is done this late to avoid
  // testing against a bunch of special cases that are taken care early on.
  PrepareRestartOnCrashEnviroment(parsed_command_line);

#if defined(OS_WIN)
  // Initialize Winsock.
  net::EnsureWinsockInit();

  // Initialize the DNS prefetch system
  chrome_browser_net::DnsPrefetcherInit dns_prefetch_init(user_prefs);
  chrome_browser_net::DnsPrefetchHostNamesAtStartup(user_prefs, local_state);

  // Init common control sex.
  INITCOMMONCONTROLSEX config;
  config.dwSize = sizeof(config);
  config.dwICC = ICC_WIN95_CLASSES;
  InitCommonControlsEx(&config);

  win_util::ScopedCOMInitializer com_initializer;

  // Init the RLZ library. This just binds the dll and schedules a task on the
  // file thread to be run sometime later. If this is the first run we record
  // the installation event.
  RLZTracker::InitRlzDelayed(base::DIR_MODULE, is_first_run);

  // Config the network module so it has access to resources.
  net::NetModule::SetResourceProvider(NetResourceProvider);

  // Register our global network handler for chrome:// and chrome-extension://
  // URLs.
  RegisterURLRequestChromeJob();
  RegisterExtensionProtocols();

  sandbox::BrokerServices* broker_services =
      parameters.sandbox_info_.BrokerServices();
  browser_process->InitBrokerServices(broker_services);
#endif

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

#if defined(OS_WIN)
  // Prepare for memory caching of SDCH dictionaries.
  SdchManager sdch_manager;  // Construct singleton database.
  sdch_manager.set_sdch_fetcher(new SdchDictionaryFetcher);
  // Use default of "" so that all domains are supported.
  std::string switch_domain("");
  if (parsed_command_line.HasSwitch(switches::kSdchFilter)) {
    switch_domain =
        WideToASCII(parsed_command_line.GetSwitchValue(switches::kSdchFilter));
  }
  sdch_manager.EnableSdchSupport(switch_domain);
#endif

  MetricsService* metrics = NULL;
  if (!parsed_command_line.HasSwitch(switches::kDisableMetrics)) {
    if (parsed_command_line.HasSwitch(switches::kMetricsRecordingOnly)) {
      local_state->transient()->SetBoolean(prefs::kMetricsReportingEnabled,
                                           false);
    }
    metrics = browser_process->metrics_service();
    DCHECK(metrics);

    // If we're testing then we don't care what the user preference is, we turn
    // on recording, but not reporting, otherwise tests fail.
    if (parsed_command_line.HasSwitch(switches::kMetricsRecordingOnly)) {
      metrics->StartRecordingOnly();
    } else {
      // If the user permits metrics reporting with the checkbox in the
      // prefs, we turn on recording.
      bool enabled = local_state->GetBoolean(prefs::kMetricsReportingEnabled);

      metrics->SetUserPermitsUpload(enabled);
      if (enabled)
        metrics->Start();
    }
  }
  InstallJankometer(parsed_command_line);

#if defined(OS_WIN)
  if (parsed_command_line.HasSwitch(switches::kDebugPrint)) {
    browser_process->print_job_manager()->set_debug_dump_path(
        parsed_command_line.GetSwitchValue(switches::kDebugPrint));
  }
#endif

  HandleErrorTestParameters(parsed_command_line);

  RecordBreakpadStatusUMA(metrics);

  int result_code = ResultCodes::NORMAL_EXIT;
  if (BrowserInit::ProcessCommandLine(parsed_command_line, L"", local_state,
                                      true, profile, &result_code)) {
#if defined(OS_WIN)
    MessageLoopForUI::current()->Run(browser_process->accelerator_handler());
#elif defined(OS_POSIX)
    MessageLoopForUI::current()->Run();
#endif
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
