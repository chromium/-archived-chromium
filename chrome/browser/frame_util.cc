// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/frame_util.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/win_util.h"
#include "chrome/app/result_codes.h"
#include "chrome/browser/app_modal_dialog_queue.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/profile_manager.h"
#include "chrome/browser/render_view_host.h"
#include "chrome/browser/web_contents.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/win_util.h"
#include "chrome/views/focus_manager.h"

// TODO(beng): clean this up
static const wchar_t* kBrowserWindowKey = L"__BROWSER_WINDOW__";

// static
BrowserWindow* FrameUtil::GetBrowserWindowForHWND(HWND hwnd) {
  if (IsWindow(hwnd)) {
    HANDLE data = GetProp(hwnd, kBrowserWindowKey);
    if (data)
      return reinterpret_cast<BrowserWindow*>(data);
  }
  return NULL;
}

// static
// TODO(beng): post new frames, move somewhere more logical, maybe Browser or
//             BrowserList.
void FrameUtil::EndSession() {
  // EndSession is invoked once per frame. Only do something the first time.
  static bool already_ended = false;
  if (already_ended)
    return;
  already_ended = true;

  browser_shutdown::OnShutdownStarting(browser_shutdown::END_SESSION);

  // Write important data first.
  g_browser_process->EndSession();

  // Close all the browsers.
  BrowserList::CloseAllBrowsers(false);

  // Send out notification. This is used during testing so that the test harness
  // can properly shutdown before we exit.
  NotificationService::current()->Notify(NOTIFY_SESSION_END,
                                         NotificationService::AllSources(),
                                         NotificationService::NoDetails());

  // And shutdown.
  browser_shutdown::Shutdown();

  // At this point the message loop is still running yet we've shut everything
  // down. If any messages are processed we'll likely crash. Exit now.
  ExitProcess(ResultCodes::NORMAL_EXIT);
}
