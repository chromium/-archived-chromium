// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <shlobj.h>

#include <sstream>

#include "chrome/browser/first_run.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "base/object_watcher.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "chrome/app/result_codes.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/hang_monitor/hung_window_detector.h"
#include "chrome/browser/importer.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/views/first_run_view.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_service.h"
#include "chrome/installer/util/shell_util.h"
#include "chrome/views/accelerator_handler.h"
#include "chrome/views/window.h"

namespace {

// The kSentinelFile file absence will tell us it is a first run.
const wchar_t kSentinelFile[] = L"First Run";

// These two names are used for upgrades in-place of the chrome exe.
const wchar_t kChromeUpgradeExe[] = L"new_chrome.exe";
const wchar_t kChromeBackupExe[] = L"old_chrome.exe";

// Gives the full path to the sentinel file. The file might not exist.
bool GetFirstRunSentinelFilePath(std::wstring* path) {
  std::wstring first_run_sentinel;
  if (!PathService::Get(base::DIR_EXE, &first_run_sentinel))
    return false;
  file_util::AppendToPath(&first_run_sentinel, kSentinelFile);
  *path = first_run_sentinel;
  return true;
}

bool GetNewerChromeFile(std::wstring* path) {
  if (!PathService::Get(base::DIR_EXE, path))
    return false;
  file_util::AppendToPath(path, kChromeUpgradeExe);
  return true;
}

bool GetBackupChromeFile(std::wstring* path) {
  if (!PathService::Get(base::DIR_EXE, path))
    return false;
  file_util::AppendToPath(path, kChromeBackupExe);
  return true;
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
  std::wstring chrome_exe, shortcut_path, shortcut_name;
  if (!PathService::Get(base::FILE_EXE, &chrome_exe) ||
      !ShellUtil::GetDesktopPath(&shortcut_path) ||
      !ShellUtil::GetChromeShortcutName(&shortcut_name))
    return false;
  file_util::AppendToPath(&shortcut_path, shortcut_name);
  return ShellUtil::UpdateChromeShortcut(chrome_exe, shortcut_path, true);
}

bool FirstRun::RemoveChromeDesktopShortcut() {
  std::wstring shortcut_path, shortcut_name;
  if (!ShellUtil::GetDesktopPath(&shortcut_path) ||
      !ShellUtil::GetChromeShortcutName(&shortcut_name))
    return false;
  file_util::AppendToPath(&shortcut_path, shortcut_name);
  return file_util::Delete(shortcut_path, false);
}

bool FirstRun::CreateChromeQuickLaunchShortcut() {
  std::wstring chrome_exe, shortcut_path, shortcut_name;
  if (!PathService::Get(base::FILE_EXE, &chrome_exe) ||
      !ShellUtil::GetQuickLaunchPath(&shortcut_path) ||
      !ShellUtil::GetChromeShortcutName(&shortcut_name))
    return false;
  file_util::AppendToPath(&shortcut_path, shortcut_name);
  return ShellUtil::UpdateChromeShortcut(chrome_exe, shortcut_path, true);
}

bool FirstRun::RemoveChromeQuickLaunchShortcut() {
  std::wstring shortcut_path, shortcut_name;
  if (!ShellUtil::GetQuickLaunchPath(&shortcut_path) ||
      !ShellUtil::GetChromeShortcutName(&shortcut_name))
    return false;
  file_util::AppendToPath(&shortcut_path, shortcut_name);
  return file_util::Delete(shortcut_path, false);
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

bool Upgrade::SwapNewChromeExeIfPresent() {
  std::wstring new_chrome_exe;
  if (!GetNewerChromeFile(&new_chrome_exe))
    return false;
  if (!file_util::PathExists(new_chrome_exe))
    return false;
  std::wstring old_chrome_exe;
  if (!PathService::Get(base::FILE_EXE, &old_chrome_exe))
    return false;
  std::wstring backup_exe;
  if (!GetBackupChromeFile(&backup_exe))
    return false;
  if (::ReplaceFileW(old_chrome_exe.c_str(), new_chrome_exe.c_str(),
                     backup_exe.c_str(), REPLACEFILE_IGNORE_MERGE_ERRORS,
                     NULL, NULL)) {
    return true;
  }
  return false;
}

bool Upgrade::RelaunchChromeBrowser(const CommandLine& command_line) {
  return process_util::LaunchApp(command_line.command_line_string(),
                                 false, false, NULL);
}

void OpenFirstRunDialog(Profile* profile) {
  ChromeViews::Window::CreateChromeWindow(NULL, gfx::Rect(),
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
  explicit ImportProcessRunner(ProcessHandle import_process)
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
  ProcessHandle import_process_;
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
  HungImporterMonitor(HWND owner_window, ProcessHandle import_process)
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
  ProcessHandle import_process_;
  WorkerThreadTicker ticker_;
  DISALLOW_EVIL_CONSTRUCTORS(HungImporterMonitor);
};

// This class is used by FirstRun::ImportWithUI to get notified of the outcome
// of the import operation. It differs from ImportProcessRunner in that this
// class executes in the context of importing child process.
// The values that it handles are meant to be used as the process exit code.
class FirstRunImportObserver : public ImportObserver {
 public:
  FirstRunImportObserver() : import_result_(ResultCodes::NORMAL_EXIT) {
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
 private:
  void Finish() {
    MessageLoop::current()->Quit();
  }

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
  CommandLine cmdline;
  std::wstring import_cmd(cmdline.program());
  // Propagate the following switches to the importer command line.
  static const wchar_t* const switch_names[] = {
    switches::kUserDataDir,
    switches::kLang,
  };
  for (int i = 0; i < arraysize(switch_names); ++i) {
    if (cmdline.HasSwitch(switch_names[i])) {
      CommandLine::AppendSwitchWithValue(
          &import_cmd, switch_names[i],
          cmdline.GetSwitchValue(switch_names[i]));
    }
  }
  CommandLine::AppendSwitchWithValue(&import_cmd, switches::kImport,
      EncodeImportParams(browser, items_to_import, parent_window));

  // Time to launch the process that is going to do the import.
  ProcessHandle import_process;
  if (!process_util::LaunchApp(import_cmd, false, false, &import_process))
    return false;

  // Activate the importer monitor. It awakes periodically in another thread
  // and checks that the importer UI is still pumping messages.
  HungImporterMonitor hang_monitor(parent_window, import_process);

  // We block inside the import_runner ctor, pumping messages until the
  // importer process ends. This can happen either by completing the import
  // or by hang_monitor killing it.
  ImportProcessRunner import_runner(import_process);

  // Import process finished. Reload the prefs, because importer may set
  // the pref value.
  profile->GetPrefs()->ReloadPersistentPrefs();
  return (import_runner.exit_code() == ResultCodes::NORMAL_EXIT);
}

int FirstRun::ImportWithUI(Profile* profile, const CommandLine& cmdline) {
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
  StartImportingWithUI(
      parent_window,
      items_to_import,
      importer_host,
      importer_host->GetSourceProfileInfoAt(browser),
      profile,
      &observer,
      true);
  MessageLoop::current()->Run();
  return observer.import_result();
}

