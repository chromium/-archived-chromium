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

#include "chrome/browser/frame_util.h"

#include "base/message_loop.h"
#include "chrome/app/result_codes.h"
#include "chrome/browser/app_modal_dialog_queue.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/profile_manager.h"
#include "chrome/browser/render_view_host.h"
#include "chrome/browser/simple_vista_frame.h"
#include "chrome/browser/simple_xp_frame.h"
#include "chrome/browser/vista_frame.h"
#include "chrome/browser/xp_frame.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/win_util.h"
#include "chrome/views/focus_manager.h"

// TODO(beng): clean this up
static const wchar_t* kBrowserWindowKey = L"__BROWSER_WINDOW__";

// static
void FrameUtil::RegisterBrowserWindow(BrowserWindow* frame) {
  DCHECK(!g_browser_process->IsUsingNewFrames());
  HWND h = reinterpret_cast<HWND>(frame->GetPlatformID());
  win_util::SetWindowUserData(h, frame);
}

// static
BrowserWindow* FrameUtil::GetBrowserWindowForHWND(HWND hwnd) {
  if (IsWindow(hwnd)) {
    if (g_browser_process->IsUsingNewFrames()) {
      HANDLE data = GetProp(hwnd, kBrowserWindowKey);
      if (data)
        return reinterpret_cast<BrowserWindow*>(data);
    } else {
      std::wstring class_name = win_util::GetClassName(hwnd);
      if (class_name == VISTA_FRAME_CLASSNAME ||
          class_name == XP_FRAME_CLASSNAME) {
        // Need to check for both, as it's possible to have vista and xp frames
        // at the same time (you can get into this state when connecting via
        // remote desktop to a vista machine with Chrome already running).
        return static_cast<BrowserWindow*>(win_util::GetWindowUserData(hwnd));
      }
    }
  }
  return NULL;
}

// static
BrowserWindow* FrameUtil::CreateBrowserWindow(const gfx::Rect& bounds,
                                              Browser* browser) {
  DCHECK(!g_browser_process->IsUsingNewFrames());

  BrowserWindow* frame = NULL;

  switch (browser->GetType()) {
    case BrowserType::TABBED_BROWSER: {
      bool is_off_the_record = browser->profile()->IsOffTheRecord();
      if (win_util::ShouldUseVistaFrame())
        frame = VistaFrame::CreateFrame(bounds, browser, is_off_the_record);
      else
        frame = XPFrame::CreateFrame(bounds, browser, is_off_the_record);
      break;
    }
    case BrowserType::APPLICATION:
    case BrowserType::BROWSER:
      if (win_util::ShouldUseVistaFrame())
        frame = SimpleVistaFrame::CreateFrame(bounds, browser);
      else
        frame = SimpleXPFrame::CreateFrame(bounds, browser);
      break;
    default:
      NOTREACHED() << "Browser type unknown or not yet implemented";
      return NULL;
  }
  frame->Init();
  return frame;
}

// static
bool FrameUtil::LoadAccelerators(BrowserWindow* frame,
    HACCEL accelerator_table,
    ChromeViews::AcceleratorTarget* accelerator_target) {
  DCHECK(!g_browser_process->IsUsingNewFrames());

  // We have to copy the table to access its contents.
  int count = CopyAcceleratorTable(accelerator_table, 0, 0);
  if (count == 0) {
    // Nothing to do in that case.
    return false;
  }

  ACCEL* accelerators = static_cast<ACCEL*>(malloc(sizeof(ACCEL) * count));
  CopyAcceleratorTable(accelerator_table, accelerators, count);

  HWND hwnd = static_cast<HWND>(frame->GetPlatformID());
  ChromeViews::FocusManager* focus_manager =
      ChromeViews::FocusManager::GetFocusManager(hwnd);
  DCHECK(focus_manager);

  // Let's build our own accelerator table.
  std::map<ChromeViews::Accelerator, int>* our_accelerators =
      new std::map<ChromeViews::Accelerator, int>;
  for (int i = 0; i < count; ++i) {
    bool alt_down = (accelerators[i].fVirt & FALT) == FALT;
    bool ctrl_down = (accelerators[i].fVirt & FCONTROL) == FCONTROL;
    bool shift_down = (accelerators[i].fVirt & FSHIFT) == FSHIFT;
    ChromeViews::Accelerator accelerator(accelerators[i].key,
                                         shift_down, ctrl_down, alt_down);
    (*our_accelerators)[accelerator] = accelerators[i].cmd;

    // Also register with the focus manager.
    focus_manager->RegisterAccelerator(accelerator, accelerator_target);
  }

  // We don't need the Windows accelerator table anymore.
  free(accelerators);

  // Now set the accelerator table on the frame who becomes the owner.
  frame->SetAcceleratorTable(our_accelerators);

  return true;
}

// static
bool FrameUtil::ActivateAppModalDialog(Browser* browser) {
  // If another browser is app modal, flash and activate the modal browser.
  if (BrowserList::IsShowingAppModalDialog()) {
    if (browser != BrowserList::GetLastActive()) {
      BrowserList::GetLastActive()->window()->FlashFrame();
      BrowserList::GetLastActive()->MoveToFront(true);
    }
    AppModalDialogQueue::ActivateModalDialog();
    return true;
  }
  return false;
}

// static
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


// static
void FrameUtil::NotifyTabsOfThemeChange(Browser* browser) {
  if (!browser) {
    NOTREACHED();
    return;
  }

  int tab_count = browser->tab_count();
  for (int tab_index = 0; tab_index < tab_count; ++tab_index) {
    TabContents* tab_contents = browser->GetTabContentsAt(tab_index);
    DCHECK(tab_contents != NULL);

    WebContents* web_contents = tab_contents->AsWebContents();
    if (!web_contents) {
      continue;
    }

    RenderViewHost* render_view_host = web_contents->render_view_host();
    if (render_view_host) {
      render_view_host->OnThemeChanged();
    }
  }
}
