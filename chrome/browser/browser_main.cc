// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <shellapi.h>

#include <algorithm>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/gfx/vector_canvas.h"
#include "base/histogram.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/registry.h"
#include "base/string_util.h"
#include "base/tracked_objects.h"
#include "base/win_util.h"
#include "chrome/app/result_codes.h"
#include "chrome/browser/automation/automation_provider.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_init.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_prefs.h"
#include "chrome/browser/browser_process_impl.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/browser_trial.h"
#include "chrome/browser/cert_store.h"
#include "chrome/browser/dom_ui/chrome_url_data_manager.h"
#include "chrome/browser/first_run.h"
#include "chrome/browser/jankometer.h"
#include "chrome/browser/metrics_service.h"
#include "chrome/browser/net/dns_global.h"
#include "chrome/browser/net/sdch_dictionary_fetcher.h"
#include "chrome/browser/plugin_service.h"
#include "chrome/browser/printing/print_job_manager.h"
#include "chrome/browser/rlz/rlz.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/url_fixer_upper.h"
#include "chrome/browser/user_metrics.h"
#include "chrome/browser/views/user_data_dir_dialog.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/env_vars.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/win_util.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/helper.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/shell_util.h"
#include "chrome/installer/util/version.h"
#include "chrome/views/accelerator_handler.h"
#include "net/base/net_module.h"
#include "net/base/net_resources.h"
#include "net/base/net_util.h"
#include "net/base/sdch_manager.h"
#include "net/base/winsock_init.h"
#include "net/http/http_network_layer.h"

#include "chromium_strings.h"
#include "generated_resources.h"

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

// This is called indirectly by the network layer to access resources.
std::string NetResourceProvider(int key) {
  const std::string& data_blob =
      ResourceBundle::GetSharedInstance().GetDataResource(key);
  if (IDR_DIR_HEADER_HTML == key) {
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
    return jstemplate_builder::GetTemplateHtml(data_blob, &value, "t");
  }

  return data_blob;
}

// Displays a warning message if the user is running chrome on windows 2000.
// Returns true if the OS is win2000, false otherwise.
bool CheckForWin2000() {
  if (win_util::GetWinVersion() == win_util::WINVERSION_2000) {
    const std::wstring text = l10n_util::GetString(IDS_UNSUPPORTED_OS_WIN2000);
    const std::wstring caption = l10n_util::GetString(IDS_PRODUCT_NAME);
    win_util::MessageBox(NULL, text, caption,
                         MB_OK | MB_ICONWARNING | MB_TOPMOST);
    return true;
  }
  return false;
}

bool AskForUninstallConfirmation() {
  const std::wstring text = l10n_util::GetString(IDS_UNINSTALL_VERIFY);
  const std::wstring caption = l10n_util::GetString(IDS_PRODUCT_NAME);
  const UINT flags = MB_OKCANCEL | MB_ICONWARNING | MB_TOPMOST;
  return (IDOK == win_util::MessageBox(NULL, text, caption, flags));
}

// Prepares the localized strings that are going to be displayed to
// the user if the browser process dies. These strings are stored in the
// environment block so they are accessible in the early stages of the
// chrome executable's lifetime.
void PrepareRestartOnCrashEnviroment(const CommandLine &parsed_command_line) {
  // Clear this var so child processes don't show the dialog by default.
  ::SetEnvironmentVariableW(env_vars::kShowRestart, NULL);

  // For non-interactive tests we don't restart on crash.
  if (::GetEnvironmentVariableW(env_vars::kHeadless, NULL, 0))
    return;

  // If the known command-line test options are used we don't create the
  // environment block which means we don't get the restart dialog.
  if (parsed_command_line.HasSwitch(switches::kBrowserCrashTest) ||
      parsed_command_line.HasSwitch(switches::kBrowserAssertTest) ||
      parsed_command_line.HasSwitch(switches::kNoErrorDialogs))
    return;

  // The encoding we use for the info is "title|context|direction" where
  // direction is either env_vars::kRtlLocale or env_vars::kLtrLocale depending
  // on the current locale.
  std::wstring dlg_strings;
  dlg_strings.append(l10n_util::GetString(IDS_CRASH_RECOVERY_TITLE));
  dlg_strings.append(L"|");
  dlg_strings.append(l10n_util::GetString(IDS_CRASH_RECOVERY_CONTENT));
  dlg_strings.append(L"|");
  if (l10n_util::GetTextDirection() == l10n_util::RIGHT_TO_LEFT)
    dlg_strings.append(env_vars::kRtlLocale);
  else
    dlg_strings.append(env_vars::kLtrLocale);

  ::SetEnvironmentVariableW(env_vars::kRestartInfo, dlg_strings.c_str());
}

int DoUninstallTasks() {
  if (!AskForUninstallConfirmation())
    return ResultCodes::UNINSTALL_USER_CANCEL;
  // The following actions are just best effort.
  LOG(INFO) << "Executing uninstall actions";
  ResultCodes::ExitCode ret = ResultCodes::NORMAL_EXIT;
  if (!FirstRun::RemoveSentinel())
    ret = ResultCodes::UNINSTALL_DELETE_FILE_ERROR;
  // We only want to modify user level shortcuts so pass false for system_level.
  if (!ShellUtil::RemoveChromeDesktopShortcut(ShellUtil::CURRENT_USER))
    ret = ResultCodes::UNINSTALL_DELETE_FILE_ERROR;
  if (!ShellUtil::RemoveChromeQuickLaunchShortcut(ShellUtil::CURRENT_USER))
    ret = ResultCodes::UNINSTALL_DELETE_FILE_ERROR;
  return ret;
}

// This method handles the --hide-icons and --show-icons command line options
// for chrome that get triggered by Windows from registry entries
// HideIconsCommand & ShowIconsCommand. Chrome doesn't support hide icons
// functionality so we just ask the users if they want to uninstall Chrome.
int HandleIconsCommands(const CommandLine &parsed_command_line) {
  if (parsed_command_line.HasSwitch(switches::kHideIcons)) {
    std::wstring cp_applet;
    if (win_util::GetWinVersion() == win_util::WINVERSION_VISTA) {
      cp_applet.assign(L"Programs and Features");  // Windows Vista and later.
    } else if (win_util::GetWinVersion() == win_util::WINVERSION_XP) {
      cp_applet.assign(L"Add/Remove Programs");  // Windows XP.
    } else {
      return ResultCodes::UNSUPPORTED_PARAM;  // Not supported
    }

    const std::wstring msg = l10n_util::GetStringF(IDS_HIDE_ICONS_NOT_SUPPORTED,
                                                   cp_applet);
    const std::wstring caption = l10n_util::GetString(IDS_PRODUCT_NAME);
    const UINT flags = MB_OKCANCEL | MB_ICONWARNING | MB_TOPMOST;
    if (IDOK == win_util::MessageBox(NULL, msg, caption, flags))
      ShellExecute(NULL, NULL, L"appwiz.cpl", NULL, NULL, SW_SHOWNORMAL);
    return ResultCodes::NORMAL_EXIT;  // Exit as we are not launching browser.
  }
  // We don't hide icons so we shouldn't do anything special to show them
  return ResultCodes::UNSUPPORTED_PARAM;
}

bool DoUpgradeTasks(const CommandLine& command_line) {
  if (!Upgrade::SwapNewChromeExeIfPresent())
    return false;
  // At this point the chrome.exe has been swapped with the new one.
  if (!Upgrade::RelaunchChromeBrowser(command_line)) {
    // The re-launch fails. Feel free to panic now.
    NOTREACHED();
  }
  return true;
}

bool CreateUniqueChromeEvent() {
  std::wstring exe;
  PathService::Get(base::FILE_EXE, &exe);
  std::replace(exe.begin(), exe.end(), '\\', '!');
  std::transform(exe.begin(), exe.end(), exe.begin(), tolower);
  HANDLE handle = CreateEvent(NULL, TRUE, TRUE, exe.c_str());
  bool already_running = false;
  if (GetLastError() == ERROR_ALREADY_EXISTS) {
    already_running = true;
    CloseHandle(handle);
  }
  return already_running;
}

// Check if there is any machine level Chrome installed on the current
// machine. If yes and the current Chrome process is user level, we do not
// allow the user level Chrome to run. So we notify the user and uninstall
// user level Chrome.
bool CheckMachineLevelInstall() {
  scoped_ptr<installer::Version> version(InstallUtil::GetChromeVersion(true));
  if (version.get()) {
    std::wstring exe;
    PathService::Get(base::DIR_EXE, &exe);
    std::transform(exe.begin(), exe.end(), exe.begin(), tolower);
    std::wstring user_exe_path = installer::GetChromeInstallPath(false);
    std::transform(user_exe_path.begin(), user_exe_path.end(),
                   user_exe_path.begin(), tolower);
    if (exe == user_exe_path) {
      const std::wstring text =
          l10n_util::GetString(IDS_MACHINE_LEVEL_INSTALL_CONFLICT);
      const std::wstring caption = l10n_util::GetString(IDS_PRODUCT_NAME);
      const UINT flags = MB_OK | MB_ICONERROR | MB_TOPMOST;
      win_util::MessageBox(NULL, text, caption, flags);
      std::wstring uninstall_cmd = InstallUtil::GetChromeUninstallCmd(false);
      if (!uninstall_cmd.empty()) {
        uninstall_cmd.append(L" --");
        uninstall_cmd.append(installer_util::switches::kForceUninstall);
        uninstall_cmd.append(L" --");
        uninstall_cmd.append(installer_util::switches::kDoNotRemoveSharedItems);
        process_util::LaunchApp(uninstall_cmd, false, false, NULL);
      }
      return true;
    }
  }
  return false;
}

// We record in UMA the conditions that can prevent breakpad from generating
// and sending crash reports. Namely that the crash reporting registration
// failed and that the process is being debugged.
void RecordBreakpadStatusUMA(MetricsService* metrics) {
  DWORD len = ::GetEnvironmentVariableW(env_vars::kNoOOBreakpad, NULL, 0);
  metrics->RecordBreakpadRegistration((len == 0));
  metrics->RecordBreakpadHasDebugger(TRUE == ::IsDebuggerPresent());
}

}  // namespace

// Main routine for running as the Browser process.
int BrowserMain(CommandLine &parsed_command_line, int show_command,
                sandbox::BrokerServices* broker_services) {
  // WARNING: If we get a WM_ENDSESSION objects created on the stack here
  // are NOT deleted. If you need something to run during WM_ENDSESSION add it
  // to browser_shutdown::Shutdown or BrowserProcess::EndSession.

  // TODO(beng, brettw): someday, break this out into sub functions with well
  //                     defined roles (e.g. pre/post-profile startup, etc).

  MessageLoop main_message_loop(MessageLoop::TYPE_UI);

  // Initialize statistical testing infrastructure.
  FieldTrialList field_trial;

  std::wstring app_name = chrome::kBrowserAppName;
  std::string thread_name_string = WideToASCII(app_name + L"_BrowserMain");

  const char* thread_name = thread_name_string.c_str();
  PlatformThread::SetName(thread_name);
  main_message_loop.set_thread_name(thread_name);
  bool already_running = CreateUniqueChromeEvent();

#if defined(OS_WIN)
  // Make the selection of network stacks early on before any consumers try to
  // issue HTTP requests.
  if (parsed_command_line.HasSwitch(switches::kUseWinHttp))
    net::HttpNetworkLayer::UseWinHttp(true);
#endif

  std::wstring user_data_dir;
  PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  BrowserInit::MessageWindow message_window(user_data_dir);

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
    FirstRun::MasterPrefResult master_pref_res =
        FirstRun::ProcessMasterPreferences(user_data_dir, std::wstring());
    first_run_ui_bypass =
        (master_pref_res == FirstRun::MASTER_PROFILE_NO_FIRST_RUN_UI);
  }

  ResourceBundle::InitSharedInstance(
      local_state->GetString(prefs::kApplicationLocale));
  // We only load the theme dll in the browser process.
  ResourceBundle::GetSharedInstance().LoadThemeResources();

  if (!parsed_command_line.HasSwitch(switches::kNoErrorDialogs)) {
    // Display a warning if the user is running windows 2000.
    CheckForWin2000();
  }

  // Initialize histogram statistics gathering system.
  StatisticsRecorder statistics;

  // Start tracking the creation and deletion of Task instances
  bool tracking_objects = false;
#ifdef TRACK_ALL_TASK_OBJECTS
  tracking_objects = tracked_objects::ThreadData::StartTracking(true);
#endif

  // Try to create/load the profile.
  ProfileManager* profile_manager = browser_process->profile_manager();
  Profile* profile = profile_manager->GetDefaultProfile(user_data_dir);
  if (!profile) {
    user_data_dir = UserDataDirDialog::RunUserDataDirDialog(user_data_dir);
    // Flush the message loop which lets the UserDataDirDialog close.
    MessageLoop::current()->Run();

    ResourceBundle::CleanupSharedInstance();

    if (!user_data_dir.empty()) {
      // Because of the way CommandLine parses, it's sufficient to append a new
      // --user-data-dir switch.  The last flag of the same name wins.
      // TODO(tc): It would be nice to remove the flag we don't want, but that
      // sounds risky if we parse differently than CommandLineToArgvW.
      std::wstring new_command_line =
          parsed_command_line.command_line_string();
      CommandLine::AppendSwitchWithValue(&new_command_line,
          switches::kUserDataDir, user_data_dir);
      process_util::LaunchApp(new_command_line, false, false, NULL);
    }

    return ResultCodes::NORMAL_EXIT;
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
      const std::wstring text = l10n_util::GetString(IDS_UNINSTALL_CLOSE_APP);
      const std::wstring caption = l10n_util::GetString(IDS_PRODUCT_NAME);
      win_util::MessageBox(NULL, text, caption,
                           MB_OK | MB_ICONWARNING | MB_TOPMOST);
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
    return FirstRun::ImportWithUI(profile, parsed_command_line);

  // When another process is running, use it instead of starting us.
  if (message_window.NotifyOtherProcess(show_command))
    return ResultCodes::NORMAL_EXIT;

  // Sometimes we end up killing browser process (http://b/1308130) so make
  // sure we recreate unique event to indicate running browser process.
  message_window.HuntForZombieChromeProcesses();
  CreateUniqueChromeEvent();

  // Do the tasks if chrome has been upgraded while it was last running.
  if (DoUpgradeTasks(parsed_command_line)) {
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

  // Initialize Winsock.
  net::EnsureWinsockInit();

  // Initialize the DNS prefetch system
  chrome_browser_net::DnsPrefetcherInit dns_prefetch_init(user_prefs);
  chrome_browser_net::DnsPretchHostNamesAtStartup(user_prefs, local_state);

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

  // Register our global network handler for chrome-resource:// URLs.
  RegisterURLRequestChromeJob();

  // TODO(brettw): we may want to move this to the browser window somewhere so
  // that if it pops up a dialog box, the user gets it as the child of the
  // browser window instead of a disembodied floating box blocking startup.
  ShellIntegration::VerifyInstallation();

  browser_process->InitBrokerServices(broker_services);

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

  // Initialize the CertStore.
  CertStore::Initialize();

  // Prepare for memory caching of SDCH dictionaries.
  SdchManager sdch_manager;  // Construct singleton database.
  if (parsed_command_line.HasSwitch(switches::kSdchFilter)) {
    sdch_manager.set_sdch_fetcher(new SdchDictionaryFetcher);
    std::wstring switch_domain =
        parsed_command_line.GetSwitchValue(switches::kSdchFilter);
    sdch_manager.EnableSdchSupport(WideToASCII(switch_domain));
  }

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

  if (parsed_command_line.HasSwitch(switches::kDebugPrint)) {
    browser_process->print_job_manager()->set_debug_dump_path(
        parsed_command_line.GetSwitchValue(switches::kDebugPrint));
  }

  HandleErrorTestParameters(parsed_command_line);

  RecordBreakpadStatusUMA(metrics);

  int result_code = ResultCodes::NORMAL_EXIT;
  if (BrowserInit::ProcessCommandLine(parsed_command_line, L"", local_state,
                                      show_command, true, profile,
                                      &result_code)) {
    MessageLoopForUI::current()->Run(browser_process->accelerator_handler());
  }

  if (metrics)
    metrics->Stop();

  // browser_shutdown takes care of deleting browser_process, so we need to
  // release it.
  browser_process.release();

  browser_shutdown::Shutdown();

  // The following teardown code will pacify Purify, but is not necessary for
  // shutdown.  Only list methods here that have no significant side effects
  // and can be run in single threaded mode before terminating.
#ifndef NDEBUG  // Don't call these in a Release build: they just waste time.
  // The following should ONLY be called when in single threaded mode. It is
  // unsafe to do this cleanup if other threads are still active.
  // It is also very unnecessary, so I'm only doing this in debug to satisfy
  // purify.
  if (tracking_objects)
    tracked_objects::ThreadData::ShutdownSingleThreadedCleanup();
#endif  // NDEBUG

  return result_code;
}

