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

#ifndef CHROME_BROWSER_IPC_STATUS_VIEW_H__
#define CHROME_BROWSER_IPC_STATUS_VIEW_H__

#include <set>

#include "base/basictypes.h"
#include "chrome/browser/status_view.h"
#include "chrome/common/ipc_logging.h"
#include "chrome/common/ipc_message_utils.h"

#ifdef IPC_MESSAGE_LOG_ENABLED

class IPCStatusView : public StatusView,
                      public IPC::Logging::Consumer {
 public:
  // button types
  enum {
    IDC_START_LOGGING = 101,
    IDC_STOP_LOGGING,
    IDC_CLEAR,
    IDC_SETTINGS,
  };

  IPCStatusView();
  virtual ~IPCStatusView();

  static IPCStatusView* current() { return current_; }
  void Log(const IPC::LogData& data);

  // TabContents overrides
  virtual const std::wstring GetDefaultTitle();
  virtual void SetActive(bool active);

  // StatusView implementation
  virtual void OnCreate(const CRect& rect);
  virtual void OnSize(const CRect& rect);

  BEGIN_MSG_MAP(IPCStatusView)
    COMMAND_HANDLER_EX(IDC_START_LOGGING, BN_CLICKED, OnStartLogging)
    COMMAND_HANDLER_EX(IDC_STOP_LOGGING, BN_CLICKED, OnStopLogging)
    COMMAND_HANDLER_EX(IDC_CLEAR, BN_CLICKED, OnClear)
    COMMAND_HANDLER_EX(IDC_SETTINGS, BN_CLICKED, OnSettings)
    CHAIN_MSG_MAP(StatusView);
  END_MSG_MAP()

  static INT_PTR CALLBACK DialogProc(
      HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
  void InitDialog(HWND hwnd);
  void CloseDialog();
  static void CreateColumn(
      uint16 start, uint16 end, HWND hwnd, CListViewCtrl** control);
  void OnCheck(int id, bool checked);
  void OnButtonClick(int id);
  static void CheckButtons(CListViewCtrl* control, bool check);

 private:

  // Event handlers
  void OnStartLogging(UINT code, int button_id, HWND hwnd);
  void OnStopLogging(UINT code, int button_id, HWND hwnd);
  void OnClear(UINT code, int button_id, HWND hwnd);
  void OnSettings(UINT code, int button_id, HWND hwnd);

  static IPCStatusView* current_;
  CListViewCtrl message_list_;

  // Used for the filter dialog.
  CListViewCtrl* view_;
  CListViewCtrl* view_host_;
  CListViewCtrl* plugin_;
  CListViewCtrl* plugin_host_;
  CListViewCtrl* npobject_;
  CListViewCtrl* plugin_process_;
  CListViewCtrl* plugin_process_host_;
  bool init_done_;
  HWND settings_dialog_;
  std::set<int> disabled_messages_;

  DISALLOW_EVIL_CONSTRUCTORS(IPCStatusView);
};

#endif // IPC_MESSAGE_LOG_ENABLED

#endif  // #ifndef CHROME_BROWSER_IPC_STATUS_VIEW_H__
