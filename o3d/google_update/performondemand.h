/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#ifndef O3D_GOOGLE_UPDATE_PERFORMONDEMAND_H_
#define O3D_GOOGLE_UPDATE_PERFORMONDEMAND_H_

#pragma once
#include <windows.h>
#include <atlbase.h>
#include <atlcom.h>
#include "google_update_idl.h"  // NOLINT

class JobObserver
  : public CComObjectRootEx<CComSingleThreadModel>,
    public IJobObserver {
 public:

  enum ReturnCodes {
    ON_COMPLETE_SUCCESS                          = 0x00000001,
    ON_COMPLETE_SUCCESS_CLOSE_UI                 = 0x00000002,
    ON_COMPLETE_ERROR                            = 0x00000004,
    ON_COMPLETE_RESTART_ALL_BROWSERS             = 0x00000008,
    ON_COMPLETE_REBOOT                           = 0x00000010,
    ON_SHOW                                      = 0x00000020,
    ON_CHECKING_FOR_UPDATES                      = 0x00000040,
    ON_UPDATE_AVAILABLE                          = 0x00000080,
    ON_WAITING_TO_DOWNLOAD                       = 0x00000100,
    ON_DOWNLOADING                               = 0x00000200,
    ON_WAITING_TO_INSTALL                        = 0x00000400,
    ON_INSTALLING                                = 0x00000800,
    ON_PAUSE                                     = 0x00001000,
    SET_EVENT_SINK                               = 0x00002000,
    ON_COMPLETE_RESTART_BROWSER                  = 0x00004000,
    ON_COMPLETE_RESTART_ALL_BROWSERS_NOTICE_ONLY = 0x00008000,
    ON_COMPLETE_REBOOT_NOTICE_ONLY               = 0x00010000,
    ON_COMPLETE_RESTART_BROWSER_NOTICE_ONLY      = 0x00020000,
    ON_COMPLETE_RUN_COMMAND                      = 0x00040000,
  };

  BEGIN_COM_MAP(JobObserver)
    COM_INTERFACE_ENTRY(IJobObserver)
  END_COM_MAP()

  // Each interaction enables a bit in observed, which is eventually returned as
  // a return code.
  int observed;

  // Similar to observed, misbehave_modes_ and close_modes_ take on bits from
  // the list of all events.  For example, if close_modes_ | ON_DOWNLOADING
  // is true, then when ON_DOWNLOADING is called, DoClose will be called.
  int misbehave_modes_;
  int close_modes_;
  bool do_closed_called;

  JobObserver()
    : observed(0), misbehave_modes_(0), close_modes_(0),
      do_closed_called(false) {
  }
  virtual ~JobObserver() {
  }

  void Reset() {
    observed = 0;
    misbehave_modes_ = 0;
    close_modes_ = 0;
    do_closed_called = false;
  }

  void AddMisbehaveMode(int event_code) {
    misbehave_modes_ |= event_code;
  }

  void AddCloseMode(int event_code) {
    close_modes_ |= event_code;
  }

  HRESULT HandleEvent(int event_code) {
    observed |= event_code;

    if ((event_code & close_modes_) && !do_closed_called) {
      do_closed_called = true;
      event_sink_->DoClose();
    }

    if (event_code & misbehave_modes_) {
      return E_FAIL;
    } else {
      return S_OK;
    }
  }

  // JobObserver implementation.
  STDMETHOD(OnShow)() {
    return HandleEvent(ON_SHOW);
  }
  STDMETHOD(OnCheckingForUpdate)() {
    return HandleEvent(ON_CHECKING_FOR_UPDATES);
  }
  STDMETHOD(OnUpdateAvailable)(const TCHAR* version_string) {
    return HandleEvent(ON_UPDATE_AVAILABLE);
  }
  STDMETHOD(OnWaitingToDownload)() {
    return HandleEvent(ON_WAITING_TO_INSTALL);
  }
  STDMETHOD(OnDownloading)(int time_remaining_ms, int pos) {
    return HandleEvent(ON_DOWNLOADING);
  }
  STDMETHOD(OnWaitingToInstall)() {
    return HandleEvent(ON_WAITING_TO_INSTALL);
  }
  STDMETHOD(OnInstalling)() {
    return HandleEvent(ON_INSTALLING);
  }
  STDMETHOD(OnPause)() {
    return HandleEvent(ON_PAUSE);
  }
  STDMETHOD(OnComplete)(CompletionCodes code, const TCHAR* text) {
    int event_code = 0;
    switch (code) {
      case COMPLETION_CODE_SUCCESS:
        event_code |= ON_COMPLETE_SUCCESS;
        break;
      case COMPLETION_CODE_SUCCESS_CLOSE_UI:
        event_code |= ON_COMPLETE_SUCCESS_CLOSE_UI;
        break;
      case COMPLETION_CODE_ERROR:
        event_code |= ON_COMPLETE_ERROR;
        break;
      case COMPLETION_CODE_RESTART_ALL_BROWSERS:
        event_code |= ON_COMPLETE_RESTART_ALL_BROWSERS;
        break;
      case COMPLETION_CODE_REBOOT:
        event_code |= ON_COMPLETE_REBOOT;
        break;
      case COMPLETION_CODE_RESTART_BROWSER:
        event_code |= ON_COMPLETE_RESTART_BROWSER;
        break;
      case COMPLETION_CODE_RESTART_ALL_BROWSERS_NOTICE_ONLY:
        event_code |= ON_COMPLETE_RESTART_ALL_BROWSERS_NOTICE_ONLY;
        break;
      case COMPLETION_CODE_REBOOT_NOTICE_ONLY:
        event_code |= ON_COMPLETE_REBOOT_NOTICE_ONLY;
        break;
      case COMPLETION_CODE_RESTART_BROWSER_NOTICE_ONLY:
        event_code |= ON_COMPLETE_RESTART_BROWSER_NOTICE_ONLY;
        break;
      case COMPLETION_CODE_RUN_COMMAND:
        event_code |= ON_COMPLETE_RUN_COMMAND;
        break;
      default:
        break;
    }
    ::PostThreadMessage(::GetCurrentThreadId(), WM_QUIT, 0, 0);
    return HandleEvent(event_code);
  }
  STDMETHOD(SetEventSink)(IProgressWndEvents* event_sink) {
    event_sink_ = event_sink;
    return HandleEvent(SET_EVENT_SINK);
  }

  CComPtr<IProgressWndEvents> event_sink_;
};

#endif  // O3D_GOOGLE_UPDATE_PERFORMONDEMAND_H_
