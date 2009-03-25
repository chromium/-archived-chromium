// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atlbase.h>
#include <atlcom.h>
#include <windows.h>
#include <shlobj.h>

#include <sstream>

#include "chrome/browser/first_run.h"

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/object_watcher.h"
#include "base/path_service.h"
#include "base/process.h"
#include "base/process_util.h"
#include "base/registry.h"
#include "base/string_util.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/hang_monitor/hung_window_detector.h"
#include "chrome/browser/importer/importer.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/profile_manager.h"
#include "chrome/browser/views/first_run_view.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/common/result_codes.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/master_preferences.h"
#include "chrome/installer/util/shell_util.h"
#include "chrome/installer/util/util_constants.h"
#include "chrome/views/widget/accelerator_handler.h"
#include "chrome/views/window/window.h"

#include "google_update_idl.h"

#include "grit/locale_settings.h"


namespace {

// The kSentinelFile file absence will tell us it is a first run.
const wchar_t kSentinelFile[] = L"First Run";

// Gives the full path to the sentinel file. The file might not exist.
bool GetFirstRunSentinelFilePath(std::wstring* path) {
  std::wstring exe_path;
  if (!PathService::Get(base::DIR_EXE, &exe_path))
    return false;

  std::wstring first_run_sentinel;
  if (InstallUtil::IsPerUserInstall(exe_path.c_str())) {
    first_run_sentinel = exe_path;
  } else {
    if (!PathService::Get(chrome::DIR_USER_DATA, &first_run_sentinel))
      return false;
  }

  file_util::AppendToPath(&first_run_sentinel, kSentinelFile);
  *path = first_run_sentinel;
  return true;
}

bool GetNewerChromeFile(std::wstring* path) {
  if (!PathService::Get(base::DIR_EXE, path))
    return false;
  file_util::AppendToPath(path, installer_util::kChromeNewExe);
  return true;
}

bool GetBackupChromeFile(std::wstring* path) {
  if (!PathService::Get(base::DIR_EXE, path))
    return false;
  file_util::AppendToPath(path, installer_util::kChromeOldExe);
  return true;
}

std::wstring GetDefaultPrefFilePath(bool create_profile_dir,
                                    const std::wstring& user_data_dir) {
  FilePath default_pref_dir = ProfileManager::GetDefaultProfileDir(
      FilePath::FromWStringHack(user_data_dir));
  if (create_profile_dir) {
    if (!file_util::PathExists(default_pref_dir)) {
      if (!file_util::CreateDirectory(default_pref_dir))
        return std::wstring();
    }
  }
  return ProfileManager::GetDefaultProfilePath(default_pref_dir)
      .ToWStringHack();
}

bool InvokeGoogleUpdateForRename() {
  CComPtr<IProcessLauncher> ipl;
  if (!FAILED(ipl.CoCreateInstance(__uuidof(ProcessLauncherClass)))) {
    ULONG_PTR phandle = NULL;
    DWORD id = GetCurrentProcessId();
    if (!FAILED(ipl->LaunchCmdElevated(google_update::kChromeGuid,
                                       google_update::kRegRenameCmdField,
                                       id, &phandle))) {
      HANDLE handle = HANDLE(phandle);
      DWORD exit_code;
      ::GetExitCodeProcess(handle, &exit_code);
      ::CloseHandle(handle);
      if (exit_code == installer_util::RENAME_SUCCESSFUL)
        return true;
    }
  }
  return false;
}

bool LaunchSetupWithParam(const std::wstring& param, const std::wstring& value,
                          int* ret_code) {
  FilePath exe_path;
  if (!PathService::Get(base::DIR_MODULE, &exe_path))
    return false;
  exe_path = exe_path.Append(installer_util::kInstallerDir);
  exe_path = exe_path.Append(installer_util::kSetupExe);
  base::ProcessHandle ph;
  CommandLine cl(exe_path.ToWStringHack());
  cl.AppendSwitchWithValue(param, value);
  if (!base::LaunchApp(cl, false, false, &ph))
    return false;
  DWORD wr = ::WaitForSingleObject(ph, INFINITE);
  if (wr != WAIT_OBJECT_0)
    return false;
  return (TRUE == ::GetExitCodeProcess(ph, reinterpret_cast<DWORD*>(ret_code)));
}

bool WriteEULAtoTempFile(FilePath* eula_path) {
  std::string terms =
      ResourceBundle::GetSharedInstance().GetDataResource(IDR_TERMS_HTML);
  if (terms.empty())
    return false;
  FilePath temp_dir;
  if (!file_util::GetTempDir(&temp_dir))
    return false;
  *eula_path = temp_dir.Append(L"chrome_eula_iframe.html");
  return (file_util::WriteFile(*eula_path, terms.c_str(), terms.size()) > 0);
}

}  // namespace

bool FirstRun::IsChromeFirstRun() {
  std::wstring first_run_sentinel;
  if (!GetFirstRunSentinelFilePath(&first_run_sentinel))
    return false;
  if (file_util::PathExists(first_run_sentinel))
    return false;
  return true;
}

bool FirstRun::CreateChromeDesktopShortcut() {
  std::wstring chrome_exe;
  if (!PathService::Get(base::FILE_EXE, &chrome_exe))
    return false;
  BrowserDistribution *dist = BrowserDistribution::GetDistribution();
  if (!dist)
    return false;
  return ShellUtil::CreateChromeDesktopShortcut(chrome_exe,
    dist->GetAppDescription(), ShellUtil::CURRENT_USER,
    false, true); // create if doesn't exist.
}

bool FirstRun::CreateChromeQuickLaunchShortcut() {
  std::wstring chrome_exe;
  if (!PathService::Get(base::FILE_EXE, &chrome_exe))
    return false;
  return ShellUtil::CreateChromeQuickLaunchShortcut(chrome_exe,
    ShellUtil::CURRENT_USER, // create only for current user.
    true); // create if doesn't exist.
}

bool FirstRun::RemoveSentinel() {
  std::wstring first_run_sentinel;
  if (!GetFirstRunSentinelFilePath(&first_run_sentinel))
    return false;
  return file_util::Delete(first_run_sentinel, false);
}

bool FirstRun::CreateSentinel() {
  std::wstring first_run_sentinel;
  if (!GetFirstRunSentinelFilePath(&first_run_sentinel))
    return false;
  HANDLE file = ::CreateFileW(first_run_sentinel.c_str(),
                              FILE_READ_DATA | FILE_WRITE_DATA,
                              FILE_SHARE_READ | FILE_SHARE_WRITE,
                              NULL, CREATE_ALWAYS, 0, NULL);
  if (INVALID_HANDLE_VALUE == file)
    return false;
  ::CloseHandle(file);
  return true;
}

bool FirstRun::ProcessMasterPreferences(
    const FilePath& user_data_dir,
    const FilePath& master_prefs_path,
    int* preference_details) {
  DCHECK(!user_data_dir.empty());
  if (preference_details)
    *preference_details = 0;

  std::wstring master_prefs;
  if (master_prefs_path.empty()) {
    // The default location of the master prefs is next to the chrome exe.
    std::wstring master_path;
    if (!PathService::Get(base::DIR_EXE, &master_path))
      return true;
    file_util::AppendToPath(&master_path, installer_util::kDefaultMasterPrefs);
    master_prefs = master_path;
  } else {
    master_prefs = master_prefs_path.ToWStringHack();
  }

  int parse_result = installer_util::ParseDistributionPreferences(master_prefs);
  if (preference_details)
    *preference_details = parse_result;

  if (parse_result & installer_util::MASTER_PROFILE_ERROR)
    return true;

  if (parse_result & installer_util::MASTER_PROFILE_REQUIRE_EULA) {
    // Show the post-installation EULA. This is done by setup.exe and the
    // result determines if we continue or not. We wait here until the user
    // dismisses the dialog.

    // The actual eula text is in a resource in chrome. We extract it to
    // a text file so setup.exe can use it as an inner frame.
    FilePath inner_html;
    if (WriteEULAtoTempFile(&inner_html)) {
      int retcode = 0;
      const std::wstring& eula =  installer_util::switches::kShowEula;
      if (!LaunchSetupWithParam(eula, inner_html.ToWStringHack(), &retcode) ||
          (retcode == installer_util::EULA_REJECTED)) {
        LOG(WARNING) << "EULA rejected. Fast exit.";
        ::ExitProcess(1);
      }
      if (retcode == installer_util::EULA_ACCEPTED) {
        LOG(INFO) << "EULA : no collection";
        GoogleUpdateSettings::SetCollectStatsConsent(false);
      } else if (retcode == installer_util::EULA_ACCEPTED_OPT_IN) {
        LOG(INFO) << "EULA : collection consent";
        GoogleUpdateSettings::SetCollectStatsConsent(true);
      }
    }
  }

  FilePath user_prefs = FilePath::FromWStringHack(
      GetDefaultPrefFilePath(true, user_data_dir.ToWStringHack()));
  if (user_prefs.empty())
    return true;

  // The master prefs are regular prefs so we can just copy the file
  // to the default place and they just work.
  if (!file_util::CopyFile(master_prefs, user_prefs.ToWStringHack()))
    return true;

  if (!(parse_result & installer_util::MASTER_PROFILE_NO_FIRST_RUN_UI))
    return true;

  // From here on we won't show first run so we need to do the work to set the
  // required state given that FirstRunView is not going to be called.
  FirstRun::SetShowFirstRunBubblePref();

  // We need to be able to create the first run sentinel or else we cannot
  // proceed because ImportSettings will launch the importer process which
  // would end up here if the sentinel is not present.
  if (!FirstRun::CreateSentinel())
    return false;

  if (parse_result & installer_util::MASTER_PROFILE_SHOW_WELCOME)
    FirstRun::SetShowWelcomePagePref();

  int import_items = 0;
  if (parse_result & installer_util::MASTER_PROFILE_IMPORT_SEARCH_ENGINE)
    import_items += SEARCH_ENGINES;
  if (parse_result & installer_util::MASTER_PROFILE_IMPORT_HISTORY)
    import_items += HISTORY;

  if (import_items) {
    // There is something to import from the default browser. This launches
    // the importer process and blocks until done or until it fails.
    if (!FirstRun::ImportSettings(NULL, 0, import_items, NULL)) {
      LOG(WARNING) << "silent import failed";
    }
  }

  return false;
}

bool Upgrade::IsBrowserAlreadyRunning() {
  static HANDLE handle = NULL;
  std::wstring exe;
  PathService::Get(base::FILE_EXE, &exe);
  std::replace(exe.begin(), exe.end(), '\\', '!');
  std::transform(exe.begin(), exe.end(), exe.begin(), tolower);
  exe = L"Global\\" + exe;
  if (handle != NULL)
    CloseHandle(handle);
  handle = CreateEvent(NULL, TRUE, TRUE, exe.c_str());
  int error = GetLastError();
  return (error == ERROR_ALREADY_EXISTS || error == ERROR_ACCESS_DENIED);
}

bool Upgrade::RelaunchChromeBrowser(const CommandLine& command_line) {
  ::SetEnvironmentVariable(google_update::kEnvProductVersionKey, NULL);
  return base::LaunchApp(command_line.command_line_string(),
                         false, false, NULL);
}

bool Upgrade::SwapNewChromeExeIfPresent() {
  std::wstring new_chrome_exe;
  if (!GetNewerChromeFile(&new_chrome_exe))
    return false;
  if (!file_util::PathExists(new_chrome_exe))
    return false;
  std::wstring curr_chrome_exe;
  if (!PathService::Get(base::FILE_EXE, &curr_chrome_exe))
    return false;

  // First try to rename exe by launching rename command ourselves.
  bool user_install = InstallUtil::IsPerUserInstall(curr_chrome_exe.c_str());
  HKEY reg_root = user_install ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;
  BrowserDistribution *dist = BrowserDistribution::GetDistribution();
  RegKey key;
  std::wstring rename_cmd;
  if (key.Open(reg_root, dist->GetVersionKey().c_str(), KEY_READ) &&
      key.ReadValue(google_update::kRegRenameCmdField, &rename_cmd)) {
    base::ProcessHandle handle;
    if (base::LaunchApp(rename_cmd, true, true, &handle)) {
      DWORD exit_code;
      ::GetExitCodeProcess(handle, &exit_code);
      ::CloseHandle(handle);
      if (exit_code == installer_util::RENAME_SUCCESSFUL)
        return true;
    }
  }

  // Rename didn't work so try to rename by calling Google Update
  if (InvokeGoogleUpdateForRename())
    return true;

  // Rename still didn't work so just try to rename exe ourselves (for
  // backward compatibility, can be deleted once the new process works).
  std::wstring backup_exe;
  if (!GetBackupChromeFile(&backup_exe))
    return false;
  if (::ReplaceFileW(curr_chrome_exe.c_str(), new_chrome_exe.c_str(),
                     backup_exe.c_str(), REPLACEFILE_IGNORE_MERGE_ERRORS,
                     NULL, NULL)) {
    return true;
  }
  return false;
}

void OpenFirstRunDialog(Profile* profile) {
  views::Window::CreateChromeWindow(NULL, gfx::Rect(),
                                    new FirstRunView(profile))->Show();
  // We must now run a message loop (will be terminated when the First Run UI
  // is closed) so that the window can receive messages and we block the
  // browser window from showing up. We pass the accelerator handler here so
  // that keyboard accelerators (Enter, Esc, etc) work in the dialog box.
  MessageLoopForUI::current()->Run(g_browser_process->accelerator_handler());
}

namespace {

// This class is used by FirstRun::ImportSettings to determine when the import
// process has ended and what was the result of the operation as reported by
// the process exit code. This class executes in the context of the main chrome
// process.
class ImportProcessRunner : public base::ObjectWatcher::Delegate {
 public:
  // The constructor takes the importer process to watch and then it does a
  // message loop blocking wait until the process ends. This object now owns
  // the import_process handle.
  explicit ImportProcessRunner(base::ProcessHandle import_process)
      : import_process_(import_process),
        exit_code_(ResultCodes::NORMAL_EXIT) {
    watcher_.StartWatching(import_process, this);
    MessageLoop::current()->Run();
  }
  virtual ~ImportProcessRunner() {
    ::CloseHandle(import_process_);
  }
  // Returns the child process exit code. There are 3 expected values:
  // NORMAL_EXIT, IMPORTER_CANCEL or IMPORTER_HUNG.
  int exit_code() const {
    return exit_code_;
  }
  // The child process has terminated. Find the exit code and quit the loop.
  virtual void OnObjectSignaled(HANDLE object) {
    DCHECK(object == import_process_);
    if (!::GetExitCodeProcess(import_process_, &exit_code_)) {
      NOTREACHED();
    }
    MessageLoop::current()->Quit();
  }

 private:
  base::ObjectWatcher watcher_;
  base::ProcessHandle import_process_;
  DWORD exit_code_;
};

// Check every 3 seconds if the importer UI has hung.
const int kPollHangFrequency = 3000;

// This class specializes on finding hung 'owned' windows. Unfortunately, the
// HungWindowDetector class cannot be used here because it assumes child
// windows and not owned top-level windows.
// This code is executed in the context of the main browser process and will
// terminate the importer process if it is hung.
class HungImporterMonitor : public WorkerThreadTicker::Callback {
 public:
  // The ctor takes the owner popup window and the process handle of the
  // process to kill in case the popup or its owned active popup become
  // unresponsive.
  HungImporterMonitor(HWND owner_window, base::ProcessHandle import_process)
      : owner_window_(owner_window),
        import_process_(import_process),
        ticker_(kPollHangFrequency) {
    ticker_.RegisterTickHandler(this);
    ticker_.Start();
  }
  virtual ~HungImporterMonitor() {
    ticker_.Stop();
    ticker_.UnregisterTickHandler(this);
  }

 private:
  virtual void OnTick() {
    if (!import_process_)
      return;
    // We find the top active popup that we own, this will be either the
    // owner_window_ itself or the dialog window of the other process. In
    // both cases it is worth hung testing because both windows share the
    // same message queue and at some point the other window could be gone
    // while the other process still not pumping messages.
    HWND active_window = ::GetLastActivePopup(owner_window_);
    if (::IsHungAppWindow(active_window) || ::IsHungAppWindow(owner_window_)) {
      ::TerminateProcess(import_process_, ResultCodes::IMPORTER_HUNG);
      import_process_ = NULL;
    }
  }

  HWND owner_window_;
  base::ProcessHandle import_process_;
  WorkerThreadTicker ticker_;
  DISALLOW_EVIL_CONSTRUCTORS(HungImporterMonitor);
};

// This class is used by FirstRun::ImportNow to get notified of the outcome of
// the import operation. It differs from ImportProcessRunner in that this
// class executes in the context of importing child process.
// The values that it handles are meant to be used as the process exit code.
class FirstRunImportObserver : public ImportObserver {
 public:
  FirstRunImportObserver()
      : loop_running_(false), import_result_(ResultCodes::NORMAL_EXIT) {
  }
  int import_result() const {
    return import_result_;
  }
  virtual void ImportCanceled() {
    import_result_ = ResultCodes::IMPORTER_CANCEL;
    Finish();
  }
  virtual void ImportComplete() {
    import_result_ = ResultCodes::NORMAL_EXIT;
    Finish();
  }

  void RunLoop() {
    loop_running_ = true;
    MessageLoop::current()->Run();
  }

 private:
  void Finish() {
    if (loop_running_)
      MessageLoop::current()->Quit();
  }

  bool loop_running_;
  int import_result_;
  DISALLOW_EVIL_CONSTRUCTORS(FirstRunImportObserver);
};

std::wstring EncodeImportParams(int browser, int options, HWND window) {
  return StringPrintf(L"%d@%d@%d", browser, options, window);
}

bool DecodeImportParams(const std::wstring& encoded,
                        int* browser, int* options, HWND* window) {
  std::vector<std::wstring> v;
  SplitString(encoded, L'@', &v);
  if (v.size() != 3)
    return false;
  *browser = static_cast<int>(StringToInt64(v[0]));
  *options = static_cast<int>(StringToInt64(v[1]));
  *window = reinterpret_cast<HWND>(StringToInt64(v[2]));
  return true;
}

}  // namespace

bool FirstRun::ImportSettings(Profile* profile, int browser,
                              int items_to_import, HWND parent_window) {
  const CommandLine& cmdline = *CommandLine::ForCurrentProcess();
  CommandLine import_cmd(cmdline.program());
  // Propagate the following switches to the importer command line.
  static const wchar_t* const switch_names[] = {
    switches::kUserDataDir,
    switches::kLang,
  };
  for (int i = 0; i < arraysize(switch_names); ++i) {
    if (cmdline.HasSwitch(switch_names[i])) {
      import_cmd.AppendSwitchWithValue(
          switch_names[i],
          cmdline.GetSwitchValue(switch_names[i]));
    }
  }
  import_cmd.CommandLine::AppendSwitchWithValue(switches::kImport,
      EncodeImportParams(browser, items_to_import, parent_window));

  // Time to launch the process that is going to do the import.
  base::ProcessHandle import_process;
  if (!base::LaunchApp(import_cmd, false, false, &import_process))
    return false;

  // Activate the importer monitor. It awakes periodically in another thread
  // and checks that the importer UI is still pumping messages.
  if (parent_window)
    HungImporterMonitor hang_monitor(parent_window, import_process);

  // We block inside the import_runner ctor, pumping messages until the
  // importer process ends. This can happen either by completing the import
  // or by hang_monitor killing it.
  ImportProcessRunner import_runner(import_process);

  // Import process finished. Reload the prefs, because importer may set
  // the pref value.
  if (profile)
    profile->GetPrefs()->ReloadPersistentPrefs();

  return (import_runner.exit_code() == ResultCodes::NORMAL_EXIT);
}

int FirstRun::ImportNow(Profile* profile, const CommandLine& cmdline) {
  std::wstring import_info = cmdline.GetSwitchValue(switches::kImport);
  if (import_info.empty()) {
    NOTREACHED();
    return false;
  }
  int browser = 0;
  int items_to_import = 0;
  HWND parent_window = NULL;
  if (!DecodeImportParams(import_info, &browser, &items_to_import,
                          &parent_window)) {
    NOTREACHED();
    return false;
  }
  scoped_refptr<ImporterHost> importer_host = new ImporterHost();
  FirstRunImportObserver observer;

  // If there is no parent window, we run in headless mode which amounts
  // to having the windows hidden and if there is user action required the
  // import is automatically canceled.
  if (!parent_window)
    importer_host->set_headless();

  StartImportingWithUI(
      parent_window,
      items_to_import,
      importer_host,
      importer_host->GetSourceProfileInfoAt(browser),
      profile,
      &observer,
      true);
  observer.RunLoop();
  return observer.import_result();
}

bool FirstRun::SetShowFirstRunBubblePref() {
  PrefService* local_state = g_browser_process->local_state();
  if (!local_state)
    return false;
  if (!local_state->IsPrefRegistered(prefs::kShouldShowFirstRunBubble)) {
    local_state->RegisterBooleanPref(prefs::kShouldShowFirstRunBubble, false);
    local_state->SetBoolean(prefs::kShouldShowFirstRunBubble, true);
  }
  return true;
}

bool FirstRun::SetShowWelcomePagePref() {
  PrefService* local_state = g_browser_process->local_state();
  if (!local_state)
    return false;
  if (!local_state->IsPrefRegistered(prefs::kShouldShowWelcomePage)) {
    local_state->RegisterBooleanPref(prefs::kShouldShowWelcomePage, false);
    local_state->SetBoolean(prefs::kShouldShowWelcomePage, true);
  }
  return true;
}
