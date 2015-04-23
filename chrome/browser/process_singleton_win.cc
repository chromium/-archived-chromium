// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/process_singleton.h"

#include "app/l10n_util.h"
#include "app/win_util.h"
#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/process_util.h"
#include "base/win_util.h"
#include "chrome/browser/browser_init.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/profile_manager.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/result_codes.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"

namespace {

// Checks the visibility of the enumerated window and signals once a visible
// window has been found.
BOOL CALLBACK BrowserWindowEnumeration(HWND window, LPARAM param) {
  bool* result = reinterpret_cast<bool*>(param);
  *result = IsWindowVisible(window) != 0;
  // Stops enumeration if a visible window has been found.
  return !*result;
}

}  // namespace

// Look for a Chrome instance that uses the same profile directory.
ProcessSingleton::ProcessSingleton(const FilePath& user_data_dir)
    : window_(NULL), locked_(false), foreground_window_(NULL) {
  // FindWindoEx and Create() should be one atomic operation in order to not
  // have a race condition.
  remote_window_ = FindWindowEx(HWND_MESSAGE, NULL, chrome::kMessageWindowClass,
                                user_data_dir.ToWStringHack().c_str());
  if (!remote_window_)
    Create();
}

ProcessSingleton::~ProcessSingleton() {
  if (window_) {
    DestroyWindow(window_);
    UnregisterClass(chrome::kMessageWindowClass, GetModuleHandle(NULL));
  }
}

bool ProcessSingleton::NotifyOtherProcess() {
  if (!remote_window_)
    return false;

  // Found another window, send our command line to it
  // format is "START\0<<<current directory>>>\0<<<commandline>>>".
  std::wstring to_send(L"START\0", 6);  // want the NULL in the string.
  std::wstring cur_dir;
  if (!PathService::Get(base::DIR_CURRENT, &cur_dir))
    return false;
  to_send.append(cur_dir);
  to_send.append(L"\0", 1);  // Null separator.
  to_send.append(GetCommandLineW());
  to_send.append(L"\0", 1);  // Null separator.

  // Allow the current running browser window making itself the foreground
  // window (otherwise it will just flash in the taskbar).
  DWORD process_id = 0;
  DWORD thread_id = GetWindowThreadProcessId(remote_window_, &process_id);
  // It is possible that the process owning this window may have died by now.
  if (!thread_id || !process_id) {
    remote_window_ = NULL;
    return false;
  }

  AllowSetForegroundWindow(process_id);

  // Gives 20 seconds timeout for the current browser process to respond.
  const int kTimeout = 20000;
  COPYDATASTRUCT cds;
  cds.dwData = 0;
  cds.cbData = static_cast<DWORD>((to_send.length() + 1) * sizeof(wchar_t));
  cds.lpData = const_cast<wchar_t*>(to_send.c_str());
  DWORD_PTR result = 0;
  if (SendMessageTimeout(remote_window_,
                         WM_COPYDATA,
                         NULL,
                         reinterpret_cast<LPARAM>(&cds),
                         SMTO_ABORTIFHUNG,
                         kTimeout,
                         &result)) {
    // It is possible that the process owning this window may have died by now.
    if (!result) {
      remote_window_ = NULL;
      return false;
    }
    return true;
  }

  // It is possible that the process owning this window may have died by now.
  if (!IsWindow(remote_window_)) {
    remote_window_ = NULL;
    return false;
  }

  // The window is hung. Scan for every window to find a visible one.
  bool visible_window = false;
  EnumThreadWindows(thread_id,
                    &BrowserWindowEnumeration,
                    reinterpret_cast<LPARAM>(&visible_window));

  // If there is a visible browser window, ask the user before killing it.
  if (visible_window) {
    std::wstring text = l10n_util::GetString(IDS_BROWSER_HUNGBROWSER_MESSAGE);
    std::wstring caption = l10n_util::GetString(IDS_PRODUCT_NAME);
    if (IDYES != win_util::MessageBox(NULL, text, caption,
                                      MB_YESNO | MB_ICONSTOP | MB_TOPMOST)) {
      // The user denied. Quit silently.
      return true;
    }
  }

  // Time to take action. Kill the browser process.
  base::KillProcessById(process_id, ResultCodes::HUNG, true);
  remote_window_ = NULL;
  return false;
}

// For windows, there is no need to call Create() since the call is made in
// the constructor but to avoid having more platform specific code in
// browser_main.cc we tolerate a second call which will do nothing.
void ProcessSingleton::Create() {
  DCHECK(!remote_window_);
  if (window_)
    return;

  HINSTANCE hinst = GetModuleHandle(NULL);

  WNDCLASSEX wc = {0};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = ProcessSingleton::WndProcStatic;
  wc.hInstance = hinst;
  wc.lpszClassName = chrome::kMessageWindowClass;
  ATOM clazz = RegisterClassEx(&wc);
  DCHECK(clazz);

  std::wstring user_data_dir;
  PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);

  // Set the window's title to the path of our user data directory so other
  // Chrome instances can decide if they should forward to us or not.
  window_ = CreateWindow(chrome::kMessageWindowClass, user_data_dir.c_str(),
                         0, 0, 0, 0, 0, HWND_MESSAGE, 0, hinst, 0);
  DCHECK(window_);

  win_util::SetWindowUserData(window_, this);
}

LRESULT ProcessSingleton::OnCopyData(HWND hwnd, const COPYDATASTRUCT* cds) {
  // If locked, it means we are not ready to process this message because
  // we are probably in a first run critical phase. We must do this before
  // doing the IsShuttingDown() check since that returns true during first run
  // (since g_browser_process hasn't been AddRefModule()d yet).
  if (locked_) {
    // Attempt to place ourselves in the foreground / flash the task bar.
    if (IsWindow(foreground_window_))
      SetForegroundWindow(foreground_window_);
    return TRUE;
  }

  // Ignore the request if the browser process is already in shutdown path.
  if (!g_browser_process || g_browser_process->IsShuttingDown()) {
    LOG(WARNING) << "Not handling WM_COPYDATA as browser is shutting down";
    return FALSE;
  }

  // We should have enough room for the shortest command (min_message_size)
  // and also be a multiple of wchar_t bytes. The shortest command
  // possible is L"START\0\0" (empty current directory and command line).
  static const int min_message_size = 7;
  if (cds->cbData < min_message_size * sizeof(wchar_t) ||
      cds->cbData % sizeof(wchar_t) != 0) {
    LOG(WARNING) << "Invalid WM_COPYDATA, length = " << cds->cbData;
    return TRUE;
  }

  // We split the string into 4 parts on NULLs.
  DCHECK(cds->lpData);
  const std::wstring msg(static_cast<wchar_t*>(cds->lpData),
                         cds->cbData / sizeof(wchar_t));
  const std::wstring::size_type first_null = msg.find_first_of(L'\0');
  if (first_null == 0 || first_null == std::wstring::npos) {
    // no NULL byte, don't know what to do
    LOG(WARNING) << "Invalid WM_COPYDATA, length = " << msg.length() <<
      ", first null = " << first_null;
    return TRUE;
  }

  // Decode the command, which is everything until the first NULL.
  if (msg.substr(0, first_null) == L"START") {
    // Another instance is starting parse the command line & do what it would
    // have done.
    LOG(INFO) << "Handling STARTUP request from another process";
    const std::wstring::size_type second_null =
      msg.find_first_of(L'\0', first_null + 1);
    if (second_null == std::wstring::npos ||
        first_null == msg.length() - 1 || second_null == msg.length()) {
      LOG(WARNING) << "Invalid format for start command, we need a string in 4 "
        "parts separated by NULLs";
      return TRUE;
    }

    // Get current directory.
    const std::wstring cur_dir =
      msg.substr(first_null + 1, second_null - first_null);

    const std::wstring::size_type third_null =
        msg.find_first_of(L'\0', second_null + 1);
    if (third_null == std::wstring::npos ||
        third_null == msg.length()) {
      LOG(WARNING) << "Invalid format for start command, we need a string in 4 "
        "parts separated by NULLs";
    }

    // Get command line.
    const std::wstring cmd_line =
      msg.substr(second_null + 1, third_null - second_null);

    CommandLine parsed_command_line(L"");
    parsed_command_line.ParseFromString(cmd_line);
    PrefService* prefs = g_browser_process->local_state();
    DCHECK(prefs);

    FilePath user_data_dir;
    PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
    ProfileManager* profile_manager = g_browser_process->profile_manager();
    Profile* profile = profile_manager->GetDefaultProfile(user_data_dir);
    if (!profile) {
      // We should only be able to get here if the profile already exists and
      // has been created.
      NOTREACHED();
      return TRUE;
    }

    // Run the browser startup sequence again, with the command line of the
    // signalling process.
    BrowserInit::ProcessCommandLine(parsed_command_line, cur_dir, false,
                                    profile, NULL);
    return TRUE;
  }
  return TRUE;
}

LRESULT CALLBACK ProcessSingleton::WndProc(HWND hwnd, UINT message,
                                           WPARAM wparam, LPARAM lparam) {
  switch (message) {
    case WM_COPYDATA:
      return OnCopyData(reinterpret_cast<HWND>(wparam),
                        reinterpret_cast<COPYDATASTRUCT*>(lparam));
    default:
      break;
  }

  return ::DefWindowProc(hwnd, message, wparam, lparam);
}
