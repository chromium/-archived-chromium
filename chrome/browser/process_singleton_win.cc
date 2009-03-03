// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/process_singleton.h"

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
#include "chrome/common/l10n_util.h"
#include "chrome/common/result_codes.h"
#include "chrome/common/win_util.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"

namespace {

// Checks the visiblilty of the enumerated window and signals once a visible
// window has been found.
BOOL CALLBACK BrowserWindowEnumeration(HWND window, LPARAM param) {
  bool* result = reinterpret_cast<bool*>(param);
  *result = IsWindowVisible(window) != 0;
  // Stops enumeration if a visible window has been found.
  return !*result;
}

}  // namespace

ProcessSingleton::ProcessSingleton(const FilePath& user_data_dir)
    : window_(NULL),
      locked_(false) {
  // Look for a Chrome instance that uses the same profile directory:
  remote_window_ = FindWindowEx(HWND_MESSAGE,
                                NULL,
                                chrome::kMessageWindowClass,
                                user_data_dir.ToWStringHack().c_str());
}

ProcessSingleton::~ProcessSingleton() {
  if (window_)
    DestroyWindow(window_);
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

void ProcessSingleton::Create() {
  DCHECK(!window_);
  DCHECK(!remote_window_);
  HINSTANCE hinst = GetModuleHandle(NULL);

  WNDCLASSEX wc = {0};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = ProcessSingleton::WndProcStatic;
  wc.hInstance = hinst;
  wc.lpszClassName = chrome::kMessageWindowClass;
  RegisterClassEx(&wc);

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
  // Ignore the request if the browser process is already in shutdown path.
  if (!g_browser_process || g_browser_process->IsShuttingDown()) {
    LOG(WARNING) << "Not handling WM_COPYDATA as browser is shutting down";
    return FALSE;
  }

  // If locked, it means we are not ready to process this message because
  // we are probably in a first run critical phase.
  if (locked_)
    return TRUE;

  // We should have enough room for the shortest command (min_message_size)
  // and also be a multiple of wchar_t bytes.
  static const int min_message_size = 7;
  if (cds->cbData < min_message_size || cds->cbData % sizeof(wchar_t) != 0) {
    LOG(WARNING) << "Invalid WM_COPYDATA, length = " << cds->cbData;
    return TRUE;
  }

  // We split the string into 4 parts on NULLs.
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
    BrowserInit::ProcessCommandLine(parsed_command_line, cur_dir, prefs, false,
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

void ProcessSingleton::HuntForZombieChromeProcesses() {
  // Detecting dead renderers is simple:
  // - The process is named chrome.exe.
  // - The process' parent doesn't exist anymore.
  // - The process doesn't have a chrome::kMessageWindowClass window.
  // If these conditions hold, the process is a zombie renderer or plugin.

  // Retrieve the list of browser processes on start. This list is then used to
  // detect zombie renderer process or plugin process.
  class ZombieDetector : public base::ProcessFilter {
   public:
    ZombieDetector() {
      for (HWND window = NULL;;) {
        window = FindWindowEx(HWND_MESSAGE,
                              window,
                              chrome::kMessageWindowClass,
                              NULL);
        if (!window)
          break;
        DWORD process = 0;
        GetWindowThreadProcessId(window, &process);
        if (process)
          browsers_.push_back(process);
      }
      // We are also a browser, regardless of having the window or not.
      browsers_.push_back(::GetCurrentProcessId());
    }

    virtual bool Includes(uint32 pid, uint32 parent_pid) const {
      // Don't kill ourself eh.
      if (GetCurrentProcessId() == pid)
        return false;

      // Is this a browser? If so, ignore it.
      if (std::find(browsers_.begin(), browsers_.end(), pid) != browsers_.end())
        return false;

      // Is the parent a browser? If so, ignore it.
      if (std::find(browsers_.begin(), browsers_.end(), parent_pid)
          != browsers_.end())
        return false;

      // The chrome process is orphan.
      return true;
    }

   protected:
    std::vector<uint32> browsers_;
  };

  ZombieDetector zombie_detector;
  base::KillProcesses(L"chrome.exe", ResultCodes::HUNG, &zombie_detector);
}
