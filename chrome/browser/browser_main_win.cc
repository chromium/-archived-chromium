// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_main_win.h"

#include <windows.h>
#include <shellapi.h>

#include "app/l10n_util.h"
#include "app/message_box_flags.h"
#include "app/win_util.h"
#include "base/command_line.h"
#include "base/path_service.h"
#include "base/win_util.h"
#include "chrome/browser/first_run.h"
#include "chrome/browser/metrics/metrics_service.h"
#include "chrome/browser/views/uninstall_dialog.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/env_vars.h"
#include "chrome/common/result_codes.h"
#include "chrome/installer/util/helper.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/shell_util.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "views/controls/message_box_view.h"
#include "views/widget/accelerator_handler.h"
#include "views/window/window.h"

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

int AskForUninstallConfirmation() {
  int ret = ResultCodes::NORMAL_EXIT;
  UninstallDialog::ShowUninstallDialog(ret);
  MessageLoopForUI::current()->Run(g_browser_process->accelerator_handler());
  return ret;
}

void ShowCloseBrowserFirstMessageBox() {
  const std::wstring text = l10n_util::GetString(IDS_UNINSTALL_CLOSE_APP);
  const std::wstring caption = l10n_util::GetString(IDS_PRODUCT_NAME);
  const UINT flags = MB_OK | MB_ICONWARNING | MB_TOPMOST;
  win_util::MessageBox(NULL, text, caption, flags);
}

int DoUninstallTasks(bool chrome_still_running) {
  if (chrome_still_running) {
    ShowCloseBrowserFirstMessageBox();
    return ResultCodes::UNINSTALL_CHROME_ALIVE;
  }
  int ret = AskForUninstallConfirmation();
  if (ret != ResultCodes::UNINSTALL_USER_CANCEL) {
    // The following actions are just best effort.
    LOG(INFO) << "Executing uninstall actions";
    if (!FirstRun::RemoveSentinel())
      LOG(INFO) << "Failed to delete sentinel file.";
    // We want to remove user level shortcuts and we only care about the ones
    // created by us and not by the installer so |alternate| is false.
    if (!ShellUtil::RemoveChromeDesktopShortcut(ShellUtil::CURRENT_USER, false))
      LOG(INFO) << "Failed to delete desktop shortcut.";
    if (!ShellUtil::RemoveChromeQuickLaunchShortcut(ShellUtil::CURRENT_USER))
      LOG(INFO) << "Failed to delete quick launch shortcut.";
  }
  return ret;
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

// This method handles the --hide-icons and --show-icons command line options
// for chrome that get triggered by Windows from registry entries
// HideIconsCommand & ShowIconsCommand. Chrome doesn't support hide icons
// functionality so we just ask the users if they want to uninstall Chrome.
int HandleIconsCommands(const CommandLine &parsed_command_line) {
  if (parsed_command_line.HasSwitch(switches::kHideIcons)) {
    std::wstring cp_applet;
    win_util::WinVersion version = win_util::GetWinVersion();
    if (version >= win_util::WINVERSION_VISTA) {
      cp_applet.assign(L"Programs and Features");  // Windows Vista and later.
    } else if (version >= win_util::WINVERSION_XP) {
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
        base::LaunchApp(uninstall_cmd, false, false, NULL);
      }
      return true;
    }
  }
  return false;
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

// We record in UMA the conditions that can prevent breakpad from generating
// and sending crash reports. Namely that the crash reporting registration
// failed and that the process is being debugged.
void RecordBreakpadStatusUMA(MetricsService* metrics) {
  DWORD len = ::GetEnvironmentVariableW(env_vars::kNoOOBreakpad, NULL, 0);
  metrics->RecordBreakpadRegistration((len == 0));
  metrics->RecordBreakpadHasDebugger(TRUE == ::IsDebuggerPresent());
}
