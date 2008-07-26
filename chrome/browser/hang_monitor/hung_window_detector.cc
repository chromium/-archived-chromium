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

#include "chrome/browser/hang_monitor/hung_window_detector.h"

#include <windows.h>

#include "base/logging.h"
#include "chrome/app/result_codes.h"

// How long do we wait for the terminated thread or process to die (in ms)
static const int kTerminateTimeout = 2000;

const wchar_t HungWindowDetector::kHungChildWindowTimeout[] =
    L"Chrome_HungChildWindowTimeout";

HungWindowDetector::HungWindowDetector(HungWindowNotification* notification)
    : notification_(notification),
      top_level_window_(NULL),
      enumerating_(false) {
  DCHECK(NULL != notification_);
}
// NOTE: It is the caller's responsibility to make sure that
// callbacks on this object have been stopped before
// destroying this object
HungWindowDetector::~HungWindowDetector() {
}

bool HungWindowDetector::Initialize(HWND top_level_window,
                                    int message_response_timeout) {
  if (NULL ==  notification_) {
    return false;
  }
  if (NULL == top_level_window) {
    return false;
  }
  // It is OK to call Initialize on this object repeatedly
  // with different top lebel HWNDs and timeout values each time.
  // And we do not need a lock for this because we are just
  // swapping DWORDs.
  top_level_window_ = top_level_window;
  message_response_timeout_ = message_response_timeout;
  return true;
}

void HungWindowDetector::OnTick() {
  do {
    AutoLock lock(hang_detection_lock_);
    // If we already are checking for hung windows on another thread,
    // don't do this again.
    if (enumerating_) {
      return;
    }
    enumerating_ = true;
  } while (false);  // To scope the AutoLock

  EnumChildWindows(top_level_window_, ChildWndEnumProc,
                   reinterpret_cast<LPARAM>(this));
  enumerating_ = false;
}

bool HungWindowDetector::CheckChildWindow(HWND child_window) {
  // It can happen that the window is DOA. It specifically happens
  // when we have just killed a plugin process and the enum is still
  // enumerating windows from that process.
  if (!IsWindow(child_window))  {
    return true;
  }

  DWORD top_level_window_thread_id =
      GetWindowThreadProcessId(top_level_window_, NULL);

  DWORD child_window_process_id = 0;
  DWORD child_window_thread_id =
      GetWindowThreadProcessId(child_window, &child_window_process_id);
  bool continue_hang_detection = true;

  if (top_level_window_thread_id != child_window_thread_id) {
    // The message timeout for a child window starts of with a default
    // value specified by the message_response_timeout_ member. It is
    // tracked by a property on the child window.
#pragma warning(disable:4311)
    int child_window_message_timeout =
        reinterpret_cast<int>(GetProp(child_window, kHungChildWindowTimeout));
#pragma warning(default:4311)
    if (!child_window_message_timeout) {
      child_window_message_timeout = message_response_timeout_;
    }

    DWORD result = 0;
    if ((0 == SendMessageTimeout(child_window,
                                 WM_NULL,
                                 0,
                                 0,
                                 SMTO_BLOCK,
                                 child_window_message_timeout,
                                 &result)) ||
        IsHungAppWindow(child_window)) {
      HungWindowNotification::ActionOnHungWindow action =
          HungWindowNotification::HUNG_WINDOW_IGNORE;
#pragma warning(disable:4312)
      SetProp(child_window, kHungChildWindowTimeout,
              reinterpret_cast<HANDLE>(child_window_message_timeout));
#pragma warning(default:4312)
      continue_hang_detection =
        notification_->OnHungWindowDetected(child_window, top_level_window_,
                                            &action);
      // Make sure this window still a child of our top-level parent
      if (!IsChild(top_level_window_, child_window)) {
        return continue_hang_detection;
      }

      switch (action) {
        case HungWindowNotification::HUNG_WINDOW_TERMINATE_THREAD: {
          CHandle child_thread(OpenThread(THREAD_TERMINATE,
                                          FALSE,
                                          child_window_thread_id));
          if (NULL == child_thread.m_h) {
            break;
          }
          // Before swinging the axe, do some sanity checks to make
          // sure this window still belongs to the same thread
          if (GetWindowThreadProcessId(child_window, NULL) !=
                  child_window_thread_id) {
            break;
          }
          TerminateThread(child_thread, 0);
          WaitForSingleObject(child_thread, kTerminateTimeout);
          child_thread.Close();
          break;
        }
        case HungWindowNotification::HUNG_WINDOW_TERMINATE_PROCESS: {
          CHandle child_process(OpenProcess(PROCESS_TERMINATE,
                                            FALSE,
                                            child_window_process_id));

          if (NULL == child_process.m_h)  {
            break;
          }
          // Before swinging the axe, do some sanity checks to make
          // sure this window still belongs to the same process
          DWORD process_id_check = 0;
          GetWindowThreadProcessId(child_window, &process_id_check);
          if (process_id_check !=  child_window_process_id) {
            break;
          }
          TerminateProcess(child_process, ResultCodes::HUNG);
          WaitForSingleObject(child_process, kTerminateTimeout);
          child_process.Close();
          break;
        }
        default: {
          break;
        }
      }
    } else {
      RemoveProp(child_window, kHungChildWindowTimeout);
    }
  }

  return continue_hang_detection;
}

BOOL CALLBACK HungWindowDetector::ChildWndEnumProc(HWND child_window,
                                                   LPARAM param) {
  HungWindowDetector* detector_instance =
      reinterpret_cast<HungWindowDetector*>(param);
  if (NULL == detector_instance) {
    NOTREACHED();
    return FALSE;
  }

  return detector_instance->CheckChildWindow(child_window);
}

