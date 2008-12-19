// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <Windowsx.h>

#include "base/registry.h"

#include "Resource.h"

// This enum needs to be in sync with the strings below.
enum Branch {
  UNKNOWN_BRANCH = 0,
  DEV_BRANCH,
  BETA_BRANCH,
  STABLE_BRANCH,
};

// This vector of strings needs to be in sync with the Branch enum above.
static const wchar_t* const kBranchStrings[] = {
  L"?",
  L"1.1-dev",
  L"1.1-beta",
  L"",
};

// This vector of strings needs to be in sync with the Branch enum above.
static const wchar_t* const kBranchStringsReadable[] = {
  L"?",
  L"Dev",
  L"Beta",
  L"Stable",
};

// The Registry Hive to write to. Points to the hive where we found the 'ap' key
// unless there is an error, in which case it is 0.
HKEY registry_hive = 0;

// The value of the 'ap' key under the registry hive specified in
// |registry_hive|.
std::wstring update_branch;

// The Google Update key to read to find out which branch you are on.
static const wchar_t* const kChromeClientStateKey =
    L"Software\\Google\\Update\\ClientState\\"
    L"{8A69D345-D564-463C-AFF1-A69D9E530F96}";

// The Google Client key to read to find out which branch you are on.
static const wchar_t* const kChromeClientsKey =
    L"Software\\Google\\Update\\Clients\\"
    L"{8A69D345-D564-463C-AFF1-A69D9E530F96}";

// The Google Update value that defines which branch you are on.
static const wchar_t* const kBranchKey = L"ap";

// The suffix Google Update sometimes adds to the channel name (channel names
// are defined in kBranchStrings), indicating that a full install is needed. We
// strip this out (if present) for the purpose of determining which channel you
// are on.
static const wchar_t* const kChannelSuffix = L"-full";

// Title to show in the MessageBoxes.
static const wchar_t* const kMessageBoxTitle = L"Google Chrome Channel Changer";

// A parameter passed into us when we are trying to elevate. This is used as a
// safeguard to make sure we only try to elevate once (so that if it fails we
// don't create an infinite process spawn loop).
static const wchar_t* const kElevationParam = L"--elevation-attempt";

// The icon to use.
static HICON dlg_icon = NULL;

// We need to detect which update branch the user is on. We do this by checking
// under the Google Update 'Clients\<Chrome GUID>' key for HKLM (for
// system-level installs) and then HKCU (for user-level installs). Once we find
// the right registry hive, we read the current value of the 'ap' key under
// Google Update 'ClientState\<Chrome GUID>' key.
void DetectBranch() {
  // See if we can find the Clients key on the HKLM branch.
  registry_hive = HKEY_LOCAL_MACHINE;
  RegKey google_update_hklm(registry_hive, kChromeClientsKey, KEY_READ);
  if (!google_update_hklm.Valid()) {
    // HKLM failed us, try the same for the HKCU branch.
    registry_hive = HKEY_CURRENT_USER;
    RegKey google_update_hkcu(registry_hive, kChromeClientsKey, KEY_READ);
    if (!google_update_hkcu.Valid()) {
      // HKCU also failed us! "Set condition 1 throughout the ship!"
      registry_hive = 0;  // Failed to find Google Update Chrome key.
      update_branch = kBranchStrings[UNKNOWN_BRANCH];
    }
  }

  if (registry_hive != 0) {
    // Now that we know which hive to use, read the 'ap' key from it.
    RegKey client_state(registry_hive, kChromeClientStateKey, KEY_READ);
    client_state.ReadValue(kBranchKey, &update_branch);

    // We look for '1.1-beta' or '1.1-dev', but Google Update might have added
    // '-full' to the channel name, which we need to strip out to determine what
    // channel you are on.
    std::wstring suffix = kChannelSuffix;
    if (update_branch.length() > suffix.length()) {
      size_t index = update_branch.rfind(suffix);
      if (index != std::wstring::npos &&
          index == update_branch.length() - suffix.length()) {
        update_branch = update_branch.substr(0, index);
      }
    }
  }
}

void SetMainLabel(HWND dialog, Branch branch) {
  std::wstring main_label = L"You are currently on ";
  if (branch == DEV_BRANCH || branch == BETA_BRANCH || branch == STABLE_BRANCH)
    main_label += std::wstring(L"the ") + kBranchStringsReadable[branch] +
                  std::wstring(L" channel");
  else
    main_label += L"NO UPDATE CHANNEL";

  main_label += L". Choose a different channel and click Update, "
                L"or click Close to stay on this channel.";

  SetWindowText(GetDlgItem(dialog, IDC_LABEL_MAIN), main_label.c_str());
}

void OnInitDialog(HWND dialog) {
  SendMessage(dialog, WM_SETICON, (WPARAM) false, (LPARAM) dlg_icon);

  Branch branch = UNKNOWN_BRANCH;
  if (update_branch == kBranchStrings[STABLE_BRANCH]) {
    branch = STABLE_BRANCH;
  } else if (update_branch == kBranchStrings[DEV_BRANCH]) {
    branch = DEV_BRANCH;
  } else if (update_branch == kBranchStrings[BETA_BRANCH]) {
    branch = BETA_BRANCH;
  } else {
    // Hide the controls we can't use.
    EnableWindow(GetDlgItem(dialog, IDOK), false);
    EnableWindow(GetDlgItem(dialog, IDC_STABLE), false);
    EnableWindow(GetDlgItem(dialog, IDC_BETA), false);
    EnableWindow(GetDlgItem(dialog, IDC_CUTTING_EDGE), false);

    MessageBox(dialog, L"KEY NOT FOUND\n\nGoogle Chrome is not installed, or "
                       L"is not using GoogleUpdate for updates.",
                       kMessageBoxTitle,
                       MB_ICONEXCLAMATION | MB_OK);
  }

  SetMainLabel(dialog, branch);

  CheckDlgButton(dialog, IDC_STABLE,
                 branch == STABLE_BRANCH ? BST_CHECKED : BST_UNCHECKED);
  CheckDlgButton(dialog, IDC_CUTTING_EDGE,
                 branch == DEV_BRANCH ? BST_CHECKED : BST_UNCHECKED);
  CheckDlgButton(dialog, IDC_BETA,
                 branch == BETA_BRANCH ? BST_CHECKED : BST_UNCHECKED);
}

INT_PTR OnCtlColorStatic(HWND dialog, WPARAM wparam, LPARAM lparam) {
  HDC hdc = reinterpret_cast<HDC>(wparam);
  HWND control_wnd = reinterpret_cast<HWND>(lparam);

  if (GetDlgItem(dialog, IDC_STABLE) == control_wnd ||
      GetDlgItem(dialog, IDC_BETA) == control_wnd ||
      GetDlgItem(dialog, IDC_CUTTING_EDGE) == control_wnd ||
      GetDlgItem(dialog, IDC_LABEL_MAIN) == control_wnd ||
      GetDlgItem(dialog, IDC_SECONDARY_LABEL) == control_wnd) {
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(0, 0, 0));
    return reinterpret_cast<INT_PTR>(GetSysColorBrush(COLOR_WINDOW));
  }

  return static_cast<INT_PTR>(FALSE);
}

void SaveChanges(HWND dialog) {
  Branch branch = UNKNOWN_BRANCH;
  if (IsDlgButtonChecked(dialog, IDC_STABLE))
    branch = STABLE_BRANCH;
  else if (IsDlgButtonChecked(dialog, IDC_BETA))
    branch = BETA_BRANCH;
  else if (IsDlgButtonChecked(dialog, IDC_CUTTING_EDGE))
    branch = DEV_BRANCH;

  if (branch != UNKNOWN_BRANCH) {
    RegKey google_update(registry_hive, kChromeClientStateKey, KEY_WRITE);
    if (!google_update.WriteValue(kBranchKey, kBranchStrings[branch])) {
      MessageBox(dialog, L"Unable to change value. Please make sure you\n"
                         L"have permission to change registry keys.",
                         L"Unable to update branch info", MB_OK | MB_ICONERROR);
    } else {
      std::wstring save_msg = L"Your changes have been saved.\nYou are now "
                              L"on the " +
                              std::wstring(kBranchStringsReadable[branch]) +
                              L" branch.";
      MessageBox(dialog, save_msg.c_str(), L"Changes were saved",
                 MB_OK | MB_ICONINFORMATION);

      SetMainLabel(dialog, branch);
    }
  }
}

INT_PTR CALLBACK DialogWndProc(HWND dialog,
                               UINT message_id,
                               WPARAM wparam,
                               LPARAM lparam) {
  UNREFERENCED_PARAMETER(lparam);

  switch (message_id) {
    case WM_INITDIALOG:
      OnInitDialog(dialog);
      return static_cast<INT_PTR>(TRUE);
    case WM_CTLCOLORSTATIC:
      return OnCtlColorStatic(dialog, wparam, lparam);
    case WM_COMMAND:
      // If the user presses the OK button.
      if (LOWORD(wparam) == IDOK) {
        SaveChanges(dialog);
        return static_cast<INT_PTR>(TRUE);
      }
      // If the user presses the Cancel button.
      if (LOWORD(wparam) == IDCANCEL) {
        ::EndDialog(dialog, LOWORD(wparam));
        return static_cast<INT_PTR>(TRUE);
      }
      break;
    case WM_ERASEBKGND:
      PAINTSTRUCT paint;
      HDC hdc = BeginPaint(dialog, &paint);
      if (!hdc)
        return static_cast<INT_PTR>(FALSE);  // We didn't handle it.

      // Fill the background with White.
      HBRUSH brush = GetStockBrush(WHITE_BRUSH);
      HGDIOBJ old_brush = SelectObject(hdc, brush);
      RECT rc;
      GetClientRect(dialog, &rc);
      FillRect(hdc, &rc, brush);

      // Clean up.
      SelectObject(hdc, old_brush);
      EndPaint(dialog, &paint);
      return static_cast<INT_PTR>(TRUE);
  }

  return static_cast<INT_PTR>(FALSE);
}

// This function checks to see if we are running elevated. This function will
// return false on error and on success will modify the parameter |elevated|
// to specify whether we are running elevated or not. This function should only
// be called for Vista or later.
bool IsRunningElevated(bool* elevated) {
  HANDLE process_token;
  if (!::OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &process_token))
    return false;

  TOKEN_ELEVATION_TYPE elevation_type = TokenElevationTypeDefault;
  DWORD size_returned = 0;
  if (!::GetTokenInformation(process_token, TokenElevationType,
      &elevation_type, sizeof(elevation_type), &size_returned)) {
    ::CloseHandle(process_token);
    return false;
  }

  ::CloseHandle(process_token);
  *elevated = (elevation_type == TokenElevationTypeFull);
  return true;
}

// This function checks to see if we need to elevate. Essentially, we need to
// elevate if ALL of the following conditions are true:
// - The OS is Vista or later.
// - UAC is enabled.
// - We are not already elevated.
// - The registry hive we are working with is HKLM.
// This function will return false on error and on success will modify the
// parameter |elevation_required| to specify whether we need to elevated or not.
bool ElevationIsRequired(bool* elevation_required) {
  *elevation_required = false;

  // First, make sure we are running on Vista or more recent.
  OSVERSIONINFO info = {0};
  info.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
  if (!::GetVersionEx(&info))
    return false;  // Failure.

  // Unless we are Vista or newer, we don't need to elevate.
  if (info.dwMajorVersion < 6)
    return true;  // No error, we just don't need to elevate.

  // Make sure UAC is not disabled.
  RegKey key(HKEY_LOCAL_MACHINE,
             L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System");
  DWORD uac_enabled;
  if (!key.ReadValueDW(L"EnableLUA", &uac_enabled))
    uac_enabled = true;  // If the value doesn't exist, then UAC is enabled.

  if (!uac_enabled)
    return true;  // No error, but UAC is disabled, so elevation is futile!

  // This is Vista or more recent, so check if already running elevated.
  bool elevated = false;
  if (!IsRunningElevated(&elevated))
    return false;  // Error checking to see if we already elevated.

  if (elevated)
    return true;  // No error, but we are already elevated.

  // We are not already running elevated, check if we found our key under HKLM
  // because then we need to elevate us so that our writes don't get
  // virtualized.
  *elevation_required = (registry_hive == HKEY_LOCAL_MACHINE);
  return true;  // Success.
}

int RelaunchProcessWithElevation() {
  // Get the path and EXE name of this process so we can relaunch it.
  wchar_t executable[MAX_PATH];
  if (!::GetModuleFileName(0, &executable[0], MAX_PATH))
    return 0;

  SHELLEXECUTEINFO info;
  ZeroMemory(&info, sizeof(info));
  info.hwnd            = GetDesktopWindow();
  info.cbSize          = sizeof(SHELLEXECUTEINFOW);
  info.lpVerb          = L"runas";  // Relaunch with elevation.
  info.lpFile          = executable;
  info.lpParameters    = kElevationParam;  // Our special notification param.
  info.nShow           = SW_SHOWNORMAL;
  return ::ShellExecuteEx(&info);
}

BOOL RestartWithElevationIfRequired(const std::wstring& cmd_line) {
  bool elevation_required = false;
  if (!ElevationIsRequired(&elevation_required)) {
    ::MessageBox(NULL, L"Cannot determine if Elevation is required",
                 kMessageBoxTitle, MB_OK | MB_ICONERROR);
    return TRUE;  // This causes the app to exit.
  }

  if (elevation_required) {
    if (cmd_line.find(kElevationParam) != std::wstring::npos) {
      // If we get here, that means we tried to elevate but it failed.
      // We break here to prevent an infinite spawning loop.
      ::MessageBox(NULL, L"Second elevation attempted", kMessageBoxTitle,
                   MB_OK | MB_ICONERROR);
      return TRUE;  // This causes the app to exit.
    }

    // Restart this application with elevation.
    if (!RelaunchProcessWithElevation()) {
      ::MessageBox(NULL, L"Elevation attempt failed", kMessageBoxTitle,
                   MB_OK | MB_ICONERROR);
    }
    return TRUE;  // This causes the app to exit.
  }

  return FALSE;  // No elevation required, Channel Changer can continue running.
}

int APIENTRY _tWinMain(HINSTANCE instance,
                       HINSTANCE previous_instance,
                       LPTSTR    cmd_line,
                       int       cmd_show) {
  UNREFERENCED_PARAMETER(previous_instance);
  UNREFERENCED_PARAMETER(cmd_show);

  // Detect which update path the user is on. This also sets the registry_hive
  // parameter to the right Registry hive, which we will use later to determine
  // if we need to elevate (Vista and later only).
  DetectBranch();

  // If we detect that we need to elevate then we will restart this process
  // as an elevated process, so all this process needs to do is exit.
  // NOTE: We need to elevate on Vista if we detect that Chrome was installed
  // system-wide (because then we'll be modifying Google Update keys under
  // HKLM). We don't want to set the elevation policy in the manifest because
  // then we block non-admin users (that want to modify user-level Chrome
  // installation) from running the channel changer.
  if (RestartWithElevationIfRequired(cmd_line))
    return TRUE;

  dlg_icon = ::LoadIcon(instance, MAKEINTRESOURCE(IDI_BRANCH_SWITCHER));

  ::DialogBox(instance,
              MAKEINTRESOURCE(IDD_MAIN_DIALOG),
              ::GetDesktopWindow(),
              DialogWndProc);

  return TRUE;
}
