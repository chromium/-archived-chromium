// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "chrome/browser/hang_monitor/hung_plugin_action.h"

#include "base/win_util.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/win_util.h"
#include "grit/generated_resources.h"
#include "webkit/glue/plugins/webplugin_delegate_impl.h"

HungPluginAction::HungPluginAction() : current_hung_plugin_window_(NULL) {
}

HungPluginAction::~HungPluginAction() {
}

bool HungPluginAction::OnHungWindowDetected(HWND hung_window,
                                            HWND top_level_window,
                                            ActionOnHungWindow* action) {
  if (NULL == action) {
    return false;
  }
  if (!IsWindow(hung_window)) {
    return false;
  }

  bool continue_hang_detection = true;

  DWORD hung_window_process_id = 0;
  DWORD top_level_window_process_id = 0;
  GetWindowThreadProcessId(hung_window, &hung_window_process_id);
  GetWindowThreadProcessId(top_level_window, &top_level_window_process_id);

  *action = HungWindowNotification::HUNG_WINDOW_IGNORE;
  if (top_level_window_process_id != hung_window_process_id) {
    if (logging::DialogsAreSuppressed()) {
      NOTREACHED() << "Terminated a hung plugin process.";
      *action = HungWindowNotification::HUNG_WINDOW_TERMINATE_PROCESS;
    } else {
      std::wstring plugin_name;
      GetPluginName(hung_window,
                    top_level_window_process_id,
                    &plugin_name);
      if (plugin_name.empty()) {
        plugin_name = l10n_util::GetString(IDS_UNKNOWN_PLUGIN_NAME);
      }
      std::wstring msg = l10n_util::GetStringF(IDS_BROWSER_HANGMONITOR,
                                               plugin_name);
      std::wstring title = l10n_util::GetString(IDS_BROWSER_HANGMONITOR_TITLE);
      // Before displaying the message box,invoke SendMessageCallback on the
      // hung window. If the callback ever hits,the window is not hung anymore
      // and we can dismiss the message box.
      SendMessageCallback(hung_window,
                          WM_NULL,
                          0,
                          0,
                          HungWindowResponseCallback,
                          reinterpret_cast<ULONG_PTR>(this));
      current_hung_plugin_window_ = hung_window;
      const UINT mb_flags = MB_YESNO | MB_ICONQUESTION | MB_SETFOREGROUND;
      if (IDYES == win_util::MessageBox(NULL, msg, title, mb_flags)) {
        *action = HungWindowNotification::HUNG_WINDOW_TERMINATE_PROCESS;
      } else {
        // If the user choses to ignore the hung window warning, the
        // message timeout for this window should be doubled. We only
        // double the timeout property on the window if the property
        // exists. The property is deleted if the window becomes
        // responsive.
        continue_hang_detection = false;
#pragma warning(disable:4311)
        int child_window_message_timeout =
            reinterpret_cast<int>(GetProp(
                hung_window, HungWindowDetector::kHungChildWindowTimeout));
#pragma warning(default:4311)
        if (child_window_message_timeout) {
          child_window_message_timeout *= 2;
#pragma warning(disable:4312)
          SetProp(hung_window, HungWindowDetector::kHungChildWindowTimeout,
                  reinterpret_cast<HANDLE>(child_window_message_timeout));
#pragma warning(default:4312)
        }
      }
      current_hung_plugin_window_ = NULL;
    }
  }
  if (HungWindowNotification::HUNG_WINDOW_TERMINATE_PROCESS == *action) {
    // Enable the top-level window just in case the plugin had been
    // displaying a modal box that had disabled the top-level window
    EnableWindow(top_level_window, TRUE);
  }
  return continue_hang_detection;
}

void HungPluginAction::OnWindowResponsive(HWND window) {
  if (window == current_hung_plugin_window_) {
    // The message timeout for this window should fallback to the default
    // timeout as this window is now responsive.
    RemoveProp(window, HungWindowDetector::kHungChildWindowTimeout);
    // The monitored plugin recovered. Let's dismiss the message box.
    EnumThreadWindows(GetCurrentThreadId(),
                      reinterpret_cast<WNDENUMPROC>(DismissMessageBox),
                      NULL);
  }
}

bool HungPluginAction::GetPluginName(HWND plugin_window,
                                     DWORD browser_process_id,
                                     std::wstring* plugin_name) {
  DCHECK(plugin_name);
  HWND window_to_check = plugin_window;
  while (NULL != window_to_check) {
    DWORD process_id = 0;
    GetWindowThreadProcessId(window_to_check, &process_id);
    if (process_id == browser_process_id) {
      // If we have reached a window the that belongs to the browser process
      // we have gone too far.
      return false;
    }
    if (WebPluginDelegateImpl::GetPluginNameFromWindow(window_to_check,
                                                       plugin_name)) {
      return true;
    }
    window_to_check = GetParent(window_to_check);
  }
  return false;
}

// static
BOOL CALLBACK HungPluginAction::DismissMessageBox(HWND window, LPARAM ignore) {
  std::wstring class_name = win_util::GetClassNameW(window);
  // #32770 is the dialog window class which is the window class of
  // the message box being displayed.
  if (class_name == L"#32770") {
    EndDialog(window, IDNO);
    return FALSE;
  }
  return TRUE;
}

// static
void CALLBACK HungPluginAction::HungWindowResponseCallback(HWND target_window,
                                                           UINT message,
                                                           ULONG_PTR data,
                                                           LRESULT result) {
  HungPluginAction* instance = reinterpret_cast<HungPluginAction*>(data);
  DCHECK(NULL != instance);
  if (NULL != instance) {
    instance->OnWindowResponsive(target_window);
  }
}
