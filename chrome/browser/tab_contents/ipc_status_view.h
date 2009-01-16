// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_IPC_STATUS_VIEW_H_
#define CHROME_BROWSER_TAB_CONTENTS_IPC_STATUS_VIEW_H_

#include <set>

#include "base/basictypes.h"
#include "chrome/browser/tab_contents/status_view.h"
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

  DISALLOW_COPY_AND_ASSIGN(IPCStatusView);
};

#endif // IPC_MESSAGE_LOG_ENABLED

#endif  // #ifndef CHROME_BROWSER_TAB_CONTENTS_IPC_STATUS_VIEW_H_

