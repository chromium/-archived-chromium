// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_run.h"

#include <atlbase.h>
#include <atlcom.h>
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>

#include <sstream>

// TODO(port): trim this include list once first run has been refactored fully.
#include "app/app_switches.h"
#include "app/resource_bundle.h"
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
#include "chrome/browser/process_singleton.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/profile_manager.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/views/first_run_view.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/result_codes.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/master_preferences.h"
#include "chrome/installer/util/shell_util.h"
#include "chrome/installer/util/util_constants.h"
#include "google_update_idl.h"
#include "grit/app_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "views/background.h"
#include "views/controls/button/image_button.h"
#include "views/controls/button/native_button.h"
#include "views/controls/button/radio_button.h"
#include "views/controls/image_view.h"
#include "views/controls/label.h"
#include "views/controls/link.h"
#include "views/grid_layout.h"
#include "views/standard_layout.h"
#include "views/widget/accelerator_handler.h"
#include "views/widget/root_view.h"
#include "views/widget/widget_win.h"
#include "views/window/window.h"

namespace {

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

FilePath GetDefaultPrefFilePath(bool create_profile_dir,
                                const FilePath& user_data_dir) {
  FilePath default_pref_dir =
      ProfileManager::GetDefaultProfileDir(user_data_dir);
  if (create_profile_dir) {
    if (!file_util::PathExists(default_pref_dir)) {
      if (!file_util::CreateDirectory(default_pref_dir))
        return FilePath();
    }
  }
  return ProfileManager::GetDefaultProfilePath(default_pref_dir);
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

bool FirstRun::CreateChromeDesktopShortcut() {
  std::wstring chrome_exe;
  if (!PathService::Get(base::FILE_EXE, &chrome_exe))
    return false;
  BrowserDistribution *dist = BrowserDistribution::GetDistribution();
  if (!dist)
    return false;
  return ShellUtil::CreateChromeDesktopShortcut(chrome_exe,
      dist->GetAppDescription(), ShellUtil::CURRENT_USER,
      false, true);  // create if doesn't exist.
}

bool FirstRun::CreateChromeQuickLaunchShortcut() {
  std::wstring chrome_exe;
  if (!PathService::Get(base::FILE_EXE, &chrome_exe))
    return false;
  return ShellUtil::CreateChromeQuickLaunchShortcut(chrome_exe,
      ShellUtil::CURRENT_USER,  // create only for current user.
      true);  // create if doesn't exist.
}


bool FirstRun::ProcessMasterPreferences(const FilePath& user_data_dir,
                                        const FilePath& master_prefs_path,
                                        int* preference_details,
                                        std::vector<std::wstring>* new_tabs) {
  DCHECK(!user_data_dir.empty());
  if (preference_details)
    *preference_details = 0;

  FilePath master_prefs = master_prefs_path;
  if (master_prefs.empty()) {
    // The default location of the master prefs is next to the chrome exe.
    // TODO(port): port installer_util and use this.
    if (!PathService::Get(base::DIR_EXE, &master_prefs))
      return true;
    master_prefs = master_prefs.Append(installer_util::kDefaultMasterPrefs);
  }

  int parse_result = installer_util::ParseDistributionPreferences(
      master_prefs.ToWStringHack());
  if (preference_details)
    *preference_details = parse_result;

  if (parse_result & installer_util::MASTER_PROFILE_ERROR)
    return true;

  if (new_tabs)
    *new_tabs = installer_util::ParseFirstRunTabs(master_prefs.ToWStringHack());

  if (parse_result & installer_util::MASTER_PROFILE_REQUIRE_EULA) {
    // Show the post-installation EULA. This is done by setup.exe and the
    // result determines if we continue or not. We wait here until the user
    // dismisses the dialog.

    // The actual eula text is in a resource in chrome. We extract it to
    // a text file so setup.exe can use it as an inner frame.
    FilePath inner_html;
    if (WriteEULAtoTempFile(&inner_html)) {
      int retcode = 0;
      const std::wstring& eula = installer_util::switches::kShowEula;
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

  if (parse_result & installer_util::MASTER_PROFILE_OEM_FIRST_RUN_BUBBLE)
    FirstRun::SetOEMFirstRunBubblePref();

  FilePath user_prefs = GetDefaultPrefFilePath(true, user_data_dir);
  if (user_prefs.empty())
    return true;

  // The master prefs are regular prefs so we can just copy the file
  // to the default place and they just work.
  if (!file_util::CopyFile(master_prefs, user_prefs))
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
  if (parse_result & installer_util::MASTER_PROFILE_IMPORT_BOOKMARKS)
    import_items += FAVORITES;
  if (parse_result & installer_util::MASTER_PROFILE_IMPORT_HOME_PAGE)
    import_items += HOME_PAGE;

  if (import_items) {
    // There is something to import from the default browser. This launches
    // the importer process and blocks until done or until it fails.
    scoped_refptr<ImporterHost> importer_host = new ImporterHost();
    if (!FirstRun::ImportSettings(NULL,
          importer_host->GetSourceProfileInfoAt(0).browser_type,
          import_items, NULL)) {
      LOG(WARNING) << "silent import failed";
    }
  }

  if (parse_result &
      installer_util::MASTER_PROFILE_MAKE_CHROME_DEFAULT_FOR_USER)
    ShellIntegration::SetAsDefaultBrowser();

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

bool OpenFirstRunDialog(Profile* profile,
                        ProcessSingleton* process_singleton) {
  DCHECK(profile);
  DCHECK(process_singleton);

  // We need the FirstRunView to outlive its parent, as we retrieve the accept
  // state from it after the dialog has been closed.
  scoped_ptr<FirstRunView> first_run_view(new FirstRunView(profile));
  first_run_view->SetParentOwned(false);
  views::Window* first_run_ui = views::Window::CreateChromeWindow(
      NULL, gfx::Rect(), first_run_view.get());
  DCHECK(first_run_ui);

  // We need to avoid dispatching new tabs when we are doing the import
  // because that will lead to data corruption or a crash. Lock() does that.
  // If a CopyData message does come in while the First Run UI is visible,
  // then we will attempt to set first_run_ui as the foreground window.
  process_singleton->Lock(first_run_ui->GetNativeWindow());

  first_run_ui->Show();

  // We must now run a message loop (will be terminated when the First Run UI
  // is closed) so that the window can receive messages and we block the
  // browser window from showing up. We pass the accelerator handler here so
  // that keyboard accelerators (Enter, Esc, etc) work in the dialog box.
  MessageLoopForUI::current()->Run(g_browser_process->accelerator_handler());
  process_singleton->Unlock();

  return first_run_view->accepted();
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

std::wstring EncodeImportParams(int browser_type, int options, HWND window) {
  return StringPrintf(L"%d@%d@%d", browser_type, options, window);
}

bool DecodeImportParams(const std::wstring& encoded,
                        int* browser_type, int* options, HWND* window) {
  std::vector<std::wstring> v;
  SplitString(encoded, L'@', &v);
  if (v.size() != 3)
    return false;
  *browser_type = static_cast<int>(StringToInt64(v[0]));
  *options = static_cast<int>(StringToInt64(v[1]));
  *window = reinterpret_cast<HWND>(StringToInt64(v[2]));
  return true;
}

}  // namespace

bool FirstRun::ImportSettings(Profile* profile, int browser_type,
                              int items_to_import, HWND parent_window) {
  const CommandLine& cmdline = *CommandLine::ForCurrentProcess();
  CommandLine import_cmd(cmdline.program());
  // Propagate user data directory switch.
  if (cmdline.HasSwitch(switches::kUserDataDir)) {
    import_cmd.AppendSwitchWithValue(
        switches::kUserDataDir,
        cmdline.GetSwitchValue(switches::kUserDataDir));
  }

  // Since ImportSettings is called before the local state is stored on disk
  // we pass the language as an argument.  GetApplicationLocale checks the
  // current command line as fallback.
  import_cmd.AppendSwitchWithValue(
      switches::kLang,
      ASCIIToWide(g_browser_process->GetApplicationLocale()));

  import_cmd.CommandLine::AppendSwitchWithValue(switches::kImport,
      EncodeImportParams(browser_type, items_to_import, parent_window));

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
  int browser_type = 0;
  int items_to_import = 0;
  HWND parent_window = NULL;
  if (!DecodeImportParams(import_info, &browser_type, &items_to_import,
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
      importer_host->GetSourceProfileInfoForBrowserType(browser_type),
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

bool FirstRun::SetOEMFirstRunBubblePref() {
  PrefService* local_state = g_browser_process->local_state();
  if (!local_state)
    return false;
  if (!local_state->IsPrefRegistered(prefs::kShouldUseOEMFirstRunBubble)) {
    local_state->RegisterBooleanPref(prefs::kShouldUseOEMFirstRunBubble,
                                     false);
    local_state->SetBoolean(prefs::kShouldUseOEMFirstRunBubble, true);
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

//////////////////////////////////////////////////////////////////////////

namespace {

// These strings are used by TryChromeDialog. They will need to be localized
// if we use it for other locales.
const wchar_t kHeading[] =
    L"You stopped using Google Chrome. Would you like to ...";
const wchar_t kGiveChromeATry[] =
    L"Give the new version a try (already installed)";
const wchar_t kNahUninstallIt[] = L"Uninstall Google Chrome";
const wchar_t kDontBugMe[] = L"Don't bug me";
const wchar_t kOKButn[] = L"OK";
const wchar_t kWhyThis[] = L"Why am I seeing this?";
const wchar_t kHelpCenterUrl[] =
    L"http://www.google.com/support/chrome/bin/answer.py?hl=en&answer=150752";


// This class displays a modal dialog using the views system. The dialog asks
// the user to give chrome another try. This class only handles the UI so the
// resulting actions are up to the caller. It looks like this:
//
//   /----------------------------------------\
//   | |icon| You stopped using Google    [x] |
//   | |icon| Chrome. Would you like to..     |
//   |        [o] Give the new version a try  |
//   |        [ ] Uninstall Google Chrome     |
//   |        [ OK ] [Don't bug me]           |
//   |        _why_am_I_seeign this?__        |
//   ------------------------------------------
class TryChromeDialog : public views::ButtonListener,
                        public views::LinkController {
 public:
  TryChromeDialog()
      : popup_(NULL),
        try_chrome_(NULL),
        kill_chrome_(NULL),
        result_(Upgrade::TD_LAST_ENUM) {
  }

  virtual ~TryChromeDialog() {
  };

  // Shows the modal dialog asking the user to try chrome. Note that the dialog
  // has no parent and it will position itself in a lower corner of the screen.
  // The dialog does not steal focus and does not have an entry in the taskbar.
  Upgrade::TryResult ShowModal() {
    using views::GridLayout;
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();

    views::ImageView* icon = new views::ImageView();
    icon->SetImage(*rb.GetBitmapNamed(IDR_PRODUCT_ICON_32));
    gfx::Size icon_size = icon->GetPreferredSize();

    // An approximate window size. After Layout() we'll get better bounds.
    gfx::Rect pos(310, 160);
    views::WidgetWin* popup = new views::WidgetWin();
    if (!popup) {
      NOTREACHED();
      return Upgrade::TD_DIALOG_ERROR;
    }
    popup->set_delete_on_destroy(true);
    popup->set_window_style(WS_POPUP | WS_CLIPCHILDREN);
    popup->set_window_ex_style(WS_EX_TOOLWINDOW);
    popup->Init(NULL, pos);

    views::RootView* root_view = popup->GetRootView();
    // The window color is a tiny bit off-white.
    root_view->set_background(
        views::Background::CreateSolidBackground(0xfc, 0xfc, 0xfc));

    views::GridLayout* layout = CreatePanelGridLayout(root_view);
    if (!layout) {
      NOTREACHED();
      return Upgrade::TD_DIALOG_ERROR;
    }
    root_view->SetLayoutManager(layout);

    views::ColumnSet* columns;
    // First row: [icon][pad][text][button].
    columns = layout->AddColumnSet(0);
    columns->AddColumn(GridLayout::LEADING, GridLayout::LEADING, 0,
                       GridLayout::FIXED, icon_size.width(),
                       icon_size.height());
    columns->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
    columns->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                       GridLayout::USE_PREF, 0, 0);
    columns->AddColumn(GridLayout::TRAILING, GridLayout::FILL, 1,
                       GridLayout::USE_PREF, 0, 0);
    // Second row: [pad][pad][radio 1].
    columns = layout->AddColumnSet(1);
    columns->AddPaddingColumn(0, icon_size.width());
    columns->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
    columns->AddColumn(GridLayout::LEADING, GridLayout::FILL, 1,
                       GridLayout::USE_PREF, 0, 0);
    // Third row: [pad][pad][radio 2].
    columns = layout->AddColumnSet(2);
    columns->AddPaddingColumn(0, icon_size.width());
    columns->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
    columns->AddColumn(GridLayout::LEADING, GridLayout::FILL, 1,
                       GridLayout::USE_PREF, 0, 0);
    // Fourth row: [pad][pad][button][pad][button].
    columns = layout->AddColumnSet(3);
    columns->AddPaddingColumn(0, icon_size.width());
    columns->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
    columns->AddColumn(GridLayout::LEADING, GridLayout::FILL, 0,
                       GridLayout::USE_PREF, 0, 0);
    columns->AddPaddingColumn(0, kRelatedButtonHSpacing);
    columns->AddColumn(GridLayout::LEADING, GridLayout::FILL, 0,
                       GridLayout::USE_PREF, 0, 0);
    // Fifth row: [pad][pad][link].
    columns = layout->AddColumnSet(4);
    columns->AddPaddingColumn(0, icon_size.width());
    columns->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
    columns->AddColumn(GridLayout::LEADING, GridLayout::FILL, 1,
                       GridLayout::USE_PREF, 0, 0);
    // First row views.
    layout->StartRow(0, 0);
    layout->AddView(icon);
    views::Label* label = new views::Label(kHeading);
    label->SetFont(rb.GetFont(ResourceBundle::MediumBoldFont));
    label->SetMultiLine(true);
    label->SizeToFit(200);
    label->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
    layout->AddView(label);
    views::ImageButton* close_button = new views::ImageButton(this);
    close_button->SetImage(views::CustomButton::BS_NORMAL,
                          rb.GetBitmapNamed(IDR_CLOSE_BAR));
    close_button->SetImage(views::CustomButton::BS_HOT,
                          rb.GetBitmapNamed(IDR_CLOSE_BAR_H));
    close_button->SetImage(views::CustomButton::BS_PUSHED,
                          rb.GetBitmapNamed(IDR_CLOSE_BAR_P));
    close_button->set_tag(BT_CLOSE_BUTTON);
    layout->AddView(close_button);
    // Second row views.
    layout->StartRowWithPadding(0, 1, 0, 10);
    try_chrome_ = new views::RadioButton(kGiveChromeATry, 1);
    try_chrome_->SetChecked(true);
    layout->AddView(try_chrome_);
    // Third row views.
    layout->StartRow(0, 2);
    kill_chrome_ = new views::RadioButton(kNahUninstallIt, 1);
    layout->AddView(kill_chrome_);
    // Fourth row views.
    layout->StartRowWithPadding(0, 3, 0, 10);
    views::Button* accept_button = new views::NativeButton(this, kOKButn);
    accept_button->set_tag(BT_OK_BUTTON);
    layout->AddView(accept_button);
    views::Button* cancel_button = new views::NativeButton(this, kDontBugMe);
    cancel_button->set_tag(BT_CLOSE_BUTTON);
    layout->AddView(cancel_button);
    // Fifth row views.
    layout->StartRowWithPadding(0, 4, 0, 10);
    views::Link* link = new views::Link(kWhyThis);
    link->SetController(this);
    layout->AddView(link);

    // We resize the window according to the layout manager. This takes into
    // account the differences between XP and Vista fonts and buttons.
    layout->Layout(root_view);
    gfx::Size preferred = layout->GetPreferredSize(root_view);
    pos = ComputeWindowPosition(preferred.width(), preferred.height());
    popup->SetBounds(pos);

    // Carve the toast shape into the window.
    SetToastRegion(popup->GetNativeView(),
                   preferred.width(), preferred.height());
    // Time to show the window in a modal loop.
    popup_ = popup;
    popup_->Show();
    MessageLoop::current()->Run();
    return result_;
  }

 protected:
  // Overridden from ButtonListener. We have two buttons and according to
  // what the user clicked we set |result_| and we should always close and
  // end the modal loop.
  virtual void ButtonPressed(views::Button* sender) {
    if (sender->tag() == BT_CLOSE_BUTTON) {
      result_ = Upgrade::TD_NOT_NOW;
    } else {
      result_ = try_chrome_->checked() ? Upgrade::TD_TRY_CHROME :
                                         Upgrade::TD_UNINSTALL_CHROME;
    }
    popup_->Close();
    MessageLoop::current()->Quit();
  }

  // Overridden from LinkController. If the user selects the link we need to
  // fire off the default browser that by some convoluted logic should not be
  // chrome.
  virtual void LinkActivated(views::Link* source, int event_flags) {
    ::ShellExecuteW(NULL, L"open", kHelpCenterUrl, NULL, NULL, SW_SHOW);
  }

 private:
  enum ButtonTags {
    BT_NONE,
    BT_CLOSE_BUTTON,
    BT_OK_BUTTON,
  };

  // Returns a screen rectangle that is fit to show the window. In particular
  // it has the following properties: a) is visible and b) is attached to
  // the bottom of the working area.
  gfx::Rect ComputeWindowPosition(int width, int height) {
    // The 'Shell_TrayWnd' is the taskbar. We like to show our window in that
    // monitor if we can. This code works even if such window is not found.
    HWND taskbar = ::FindWindowW(L"Shell_TrayWnd", NULL);
    HMONITOR monitor =
        ::MonitorFromWindow(taskbar, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO info = {sizeof(info)};
    if (!GetMonitorInfoW(monitor, &info)) {
      // Quite unexpected. Do a best guess at a visible rectangle.
      return gfx::Rect(20, 20, width + 20, height + 20);
    }
    // The |rcWork| is the work area. It should account for the taskbars that
    // are in the screen when we called the function.
    int left = info.rcWork.right - width;
    int top = info.rcWork.bottom - height;
    return gfx::Rect(left, top, width, height);
  }

  // Create a windows region that looks like a toast of width |w| and
  // height |h|. This is best effort, so we don't care much if the operation
  // fails.
  void SetToastRegion(HWND window, int w, int h) {
    static const POINT polygon[] = {
      {0,   4}, {1,   2}, {2,   1}, {4, 0},   // Left side.
      {w-4, 0}, {w-2, 1}, {w-1, 2}, {w, 4},   // Right side.
      {w, h}, {0, h}
    };
    HRGN region = ::CreatePolygonRgn(polygon, arraysize(polygon), WINDING);
    ::SetWindowRgn(window, region, FALSE);
  }

  // We don't own any of this pointers. The |popup_| owns itself and owns
  // the other views.
  views::WidgetWin* popup_;
  views::RadioButton* try_chrome_;
  views::RadioButton* kill_chrome_;
  Upgrade::TryResult result_;

  DISALLOW_COPY_AND_ASSIGN(TryChromeDialog);
};

}  // namespace

Upgrade::TryResult Upgrade::ShowTryChromeDialog() {
  TryChromeDialog td;
  return td.ShowModal();
}
