// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <Windowsx.h>

#include "base/registry.h"

#include "Resource.h"

// This enum needs to be in sync with the strings below.
enum Branch {
  UNKNOWN_BRANCH = 0,
  DEV_BRANCH,
  BETA_BRANCH,
};

// This vector of strings needs to be in sync with the Branch enum above.
static const wchar_t* const kBranchStrings[] = {
  L"",
  L"1.1-dev",
  L"1.1-beta"
};

// This vector of strings needs to be in sync with the Branch enum above.
static const wchar_t* const kBranchStringsReadable[] = {
  L"",
  L"Dev",
  L"Beta"
};

// The root key for Google Update.
static const HKEY kGoogleUpdateRoot = HKEY_CURRENT_USER;

// The Google Update key to read to find out which branch you are on.
static const wchar_t* const kGoogleUpdateKey =
    L"Software\\Google\\Update\\ClientState\\"
    L"{8A69D345-D564-463C-AFF1-A69D9E530F96}";

// The Google Update value that defines which branch you are on.
static const wchar_t* const kBranchKey = L"ap";

// The suffix Google Update sometimes adds to the channel name (channel names
// are defined in kBranchStrings), indicating that a full install is needed. We 
// strip this out (if present) for the purpose of determining which channel you
// are on.
static const wchar_t* const kChannelSuffix = L"-full";

// The icon to use.
static HICON dlg_icon = NULL;

void SetMainLabel(HWND dialog, Branch branch) {
  std::wstring main_label = L"You are currently on ";
  if (branch == DEV_BRANCH || branch == BETA_BRANCH)
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

  std::wstring branch_string;
  RegKey google_update(kGoogleUpdateRoot, kGoogleUpdateKey, KEY_READ);
  if (google_update.Valid() &&
      !google_update.ReadValue(kBranchKey, &branch_string)) {
    // If the 'ap' value is missing, we create it, unless the key is missing.
    RegKey write_default(kGoogleUpdateRoot, kGoogleUpdateKey, KEY_WRITE);
    branch_string = kBranchStrings[BETA_BRANCH];
    if (!write_default.WriteValue(kBranchKey, branch_string.c_str()))
      branch_string = L"";  // Error, show disabled UI.
  }

  // We look for '1.1-beta' or '1.1-dev', but Google Update might have added
  // '-full' to the channel name, which we need to strip out to determine what
  // channel you are on.
  std::wstring suffix = kChannelSuffix;
  if (branch_string.length() > suffix.length()) {
    size_t index = branch_string.rfind(suffix);
    if (index != std::wstring::npos &&
        index == branch_string.length() - suffix.length()) {
      branch_string = branch_string.substr(0, index);
    }
  }

  Branch branch = UNKNOWN_BRANCH;
  if (branch_string == kBranchStrings[DEV_BRANCH]) {
    branch = DEV_BRANCH;
  } else if (branch_string == kBranchStrings[BETA_BRANCH]) {
    branch = BETA_BRANCH;
  } else {
    // Hide the controls we can't use.
    EnableWindow(GetDlgItem(dialog, IDOK), false);
    EnableWindow(GetDlgItem(dialog, IDC_STABLE), false);
    EnableWindow(GetDlgItem(dialog, IDC_CUTTING_EDGE), false);

    MessageBox(dialog, L"KEY NOT FOUND\n\nChrome is not installed, or is not "
                       L"using GoogleUpdate for updates.",
                       L"Chrome Channel Changer",
                       MB_ICONEXCLAMATION | MB_OK);
  }

  SetMainLabel(dialog, branch);

  CheckDlgButton(dialog, IDC_STABLE,
                 branch == BETA_BRANCH ? BST_CHECKED : BST_UNCHECKED);
  CheckDlgButton(dialog, IDC_CUTTING_EDGE,
                 branch == DEV_BRANCH ? BST_CHECKED : BST_UNCHECKED);
}

INT_PTR OnCtlColorStatic(HWND dialog, WPARAM wparam, LPARAM lparam) {
  HDC hdc = reinterpret_cast<HDC>(wparam);
  HWND control_wnd = reinterpret_cast<HWND>(lparam);

  if (GetDlgItem(dialog, IDC_STABLE) == control_wnd ||
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
    branch = BETA_BRANCH;
  else if (IsDlgButtonChecked(dialog, IDC_CUTTING_EDGE))
    branch = DEV_BRANCH;

  if (branch != UNKNOWN_BRANCH) {
    RegKey google_update(kGoogleUpdateRoot, kGoogleUpdateKey, KEY_WRITE);
    if (!google_update.WriteValue(kBranchKey, kBranchStrings[branch])) {
      MessageBox(dialog, L"Unable to change value, please make sure you\n"
                         L"have permission to change registry keys under HKLM",
                         L"Unable to update branch info", MB_OK);
    } else {
      std::wstring save_msg = L"Your changes have been saved.\nYou are now "
                              L"on the " +
                              std::wstring(kBranchStringsReadable[branch]) +
                              L" branch.";
      MessageBox(dialog, save_msg.c_str(), L"Changes were saved", MB_OK);

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

int APIENTRY _tWinMain(HINSTANCE instance,
                       HINSTANCE previous_instance,
                       LPTSTR    cmd_line,
                       int       cmd_show) {
  UNREFERENCED_PARAMETER(previous_instance);
  UNREFERENCED_PARAMETER(cmd_line);
  UNREFERENCED_PARAMETER(cmd_show);

  dlg_icon = ::LoadIcon(instance, MAKEINTRESOURCE(IDI_BRANCH_SWITCHER));

  ::DialogBox(instance,
              MAKEINTRESOURCE(IDD_MAIN_DIALOG),
              GetDesktopWindow(),
              DialogWndProc);

  return TRUE;
}
