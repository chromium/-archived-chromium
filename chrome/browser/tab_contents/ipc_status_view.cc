// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Need to include this before any other file because it defines
// IPC_MESSAGE_LOG_ENABLED.
#include "chrome/common/ipc_message.h"

#ifdef IPC_MESSAGE_LOG_ENABLED
#define IPC_MESSAGE_MACROS_LOG_ENABLED

#include "chrome/browser/tab_contents/ipc_status_view.h"

#include <stdio.h>

#include "base/logging.h"
#include "base/string_util.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/profile.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/ipc_logging.h"
#include "chrome/common/plugin_messages.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/render_messages.h"

using base::Time;

namespace {
const wchar_t kTitleMsg[] = L"IPC Messages";

const wchar_t kStartLoggingMsg[] = L"Start IPC Logging";
const wchar_t kStopLoggingMsg[]  = L"Stop IPC Logging";
const wchar_t kClearMsg[]        = L"Clear";
const wchar_t kSettingsMsg[]     = L"Filter";

enum {
  kTimeColumn = 0,
  kChannelColumn,
  kMessageColumn,
  kFlagsColumn,
  kDispatchColumn,
  kProcessColumn,
  kParamsColumn,
};

}  // namespace

IPCStatusView* IPCStatusView::current_;

IPCStatusView::IPCStatusView()
    : StatusView(TAB_CONTENTS_IPC_STATUS_VIEW) {
  DCHECK(!current_);
  current_ = this;
  settings_dialog_ = NULL;
  init_done_ = false;
  view_ = NULL;
  view_host_ = NULL;
  plugin_ = NULL;
  plugin_host_ = NULL;
  npobject_ = NULL;
  plugin_process_ = NULL;
  plugin_process_host_ = NULL;

  IPC::Logging* log = IPC::Logging::current();
  log->RegisterMessageLogger(ViewStart, ViewMsgLog);
  log->RegisterMessageLogger(ViewHostStart, ViewHostMsgLog);
  log->RegisterMessageLogger(PluginProcessStart, PluginProcessMsgLog);
  log->RegisterMessageLogger(PluginProcessHostStart, PluginProcessHostMsgLog);
  log->RegisterMessageLogger(PluginStart, PluginMsgLog);
  log->RegisterMessageLogger(PluginHostStart, PluginHostMsgLog);
  log->RegisterMessageLogger(NPObjectStart, NPObjectMsgLog);

  log->SetConsumer(this);
}

IPCStatusView::~IPCStatusView() {
  current_ = NULL;
  IPC::Logging::current()->SetConsumer(NULL);

  if (settings_dialog_ != NULL)
    ::DestroyWindow(settings_dialog_);
}

const std::wstring IPCStatusView::GetDefaultTitle() {
  return kTitleMsg;
}

void IPCStatusView::SetActive(bool active) {
  StatusView::set_is_active(active);

  if (!disabled_messages_.empty() || !active)
    return;

  Profile* current_profile = profile();
  if (!current_profile)
    return;
  PrefService* prefs = current_profile->GetPrefs();
  if (prefs->IsPrefRegistered(prefs::kIpcDisabledMessages))
    return;
  prefs->RegisterListPref(prefs::kIpcDisabledMessages);
  const ListValue* list = prefs->GetList(prefs::kIpcDisabledMessages);
  if (!list)
    return;
  for (ListValue::const_iterator itr = list->begin();
       itr != list->end();
       ++itr) {
    if (!(*itr)->IsType(Value::TYPE_INTEGER))
      continue;
    int value = 0;
    if (!(*itr)->GetAsInteger(&value))
      continue;
    disabled_messages_.insert(value);
  }
}

void IPCStatusView::OnCreate(const CRect& rect) {
  CreateButton(IDC_START_LOGGING, kStartLoggingMsg);
  CreateButton(IDC_STOP_LOGGING, kStopLoggingMsg);
  CreateButton(IDC_CLEAR, kClearMsg);
  CreateButton(IDC_SETTINGS, kSettingsMsg);

  // Initialize the list view for messages.
  // Don't worry about the size, we'll resize when we get WM_SIZE
  message_list_.Create(GetContainerHWND(), const_cast<CRect&>(rect), NULL,
      WS_CHILD | WS_VISIBLE | LVS_SORTASCENDING);
  message_list_.SetViewType(LVS_REPORT);
  message_list_.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT);

  int column_index = 0;
  message_list_.InsertColumn(kTimeColumn, L"time", LVCFMT_LEFT, 80);
  message_list_.InsertColumn(kChannelColumn, L"channel", LVCFMT_LEFT, 110);
  message_list_.InsertColumn(kMessageColumn, L"message", LVCFMT_LEFT, 240);
  message_list_.InsertColumn(kFlagsColumn, L"flags", LVCFMT_LEFT, 50);
  message_list_.InsertColumn(kDispatchColumn, L"dispatch (ms)", LVCFMT_RIGHT, 80);
  message_list_.InsertColumn(kProcessColumn, L"process (ms)", LVCFMT_RIGHT, 80);
  message_list_.InsertColumn(kParamsColumn, L"parameters", LVCFMT_LEFT, 500);
}

void IPCStatusView::Log(const IPC::LogData& data) {
  if (disabled_messages_.find(data.type) != disabled_messages_.end())
    return;  // Message type is filtered out.

  Time sent = Time::FromInternalValue(data.sent);
  Time::Exploded exploded;
  sent.LocalExplode(&exploded);
  if (exploded.hour > 12)
    exploded.hour -= 12;

  std::wstring sent_str = StringPrintf(L"%02d:%02d:%02d.%03d",
      exploded.hour, exploded.minute, exploded.second, exploded.millisecond);

  int count = message_list_.GetItemCount();
  int index = message_list_.InsertItem(count, sent_str.c_str());

  message_list_.SetItemText(index, kTimeColumn, sent_str.c_str());
  message_list_.SetItemText(index, kChannelColumn, data.channel.c_str());

  std::wstring message_name;
  IPC::Logging::GetMessageText(data.type, &message_name, NULL, NULL);
  message_list_.SetItemText(index, kMessageColumn, message_name.c_str());
  message_list_.SetItemText(index, kFlagsColumn, data.flags.c_str());

  int64 time_to_send = (Time::FromInternalValue(data.receive) -
      sent).InMilliseconds();
  // time can go backwards by a few ms (see Time), don't show that.
  time_to_send = std::max(static_cast<int>(time_to_send), 0);
  std::wstring temp = StringPrintf(L"%d", time_to_send);
  message_list_.SetItemText(index, kDispatchColumn, temp.c_str());

  int64 time_to_process = (Time::FromInternalValue(data.dispatch) -
      Time::FromInternalValue(data.receive)).InMilliseconds();
  time_to_process = std::max(static_cast<int>(time_to_process), 0);
  temp = StringPrintf(L"%d", time_to_process);
  message_list_.SetItemText(index, kProcessColumn, temp.c_str());

  message_list_.SetItemText(index, kParamsColumn, data.params.c_str());
  message_list_.EnsureVisible(index, FALSE);
}

void IPCStatusView::OnSize(const CRect& rect) {
  message_list_.MoveWindow(rect);
}

void IPCStatusView::OnStartLogging(UINT code, int button_id, HWND hwnd) {
  IPC::Logging::current()->Enable();
}

void IPCStatusView::OnStopLogging(UINT code, int button_id, HWND hwnd) {
  IPC::Logging::current()->Disable();
}

void IPCStatusView::OnClear(UINT code, int button_id, HWND hwnd) {
  message_list_.DeleteAllItems();
}

INT_PTR CALLBACK IPCStatusView::DialogProc(
    HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
  switch (msg) {
   case WM_INITDIALOG:
    current()->InitDialog(hwnd);
    return FALSE;  // Don't set keyboard focus.
   case WM_SYSCOMMAND:
    if (wparam == SC_CLOSE) {
      current()->CloseDialog();
      return FALSE;
    }
    break;
   case WM_NOTIFY: {
    NMLISTVIEW* info = reinterpret_cast<NM_LISTVIEW*>(lparam);
    if ((wparam == IDC_View || wparam == IDC_ViewHost || wparam == IDC_Plugin ||
         wparam == IDC_PluginHost || wparam == IDC_NPObject ||
         wparam == IDC_PluginProcess || wparam == IDC_PluginProcessHost) &&
        info->hdr.code == LVN_ITEMCHANGED) {
      if (info->uChanged & LVIF_STATE) {
        bool checked = (info->uNewState >> 12) == 2;
        current()->OnCheck(static_cast<int>(info->lParam), checked);
      }
      return FALSE;
    }
    break;
   }
   case WM_COMMAND:
    if (HIWORD(wparam) == BN_CLICKED)
      current()->OnButtonClick(LOWORD(wparam));

    break;
  }
  return FALSE;
}

void IPCStatusView::InitDialog(HWND hwnd) {
  CreateColumn(ViewStart, ViewEnd, ::GetDlgItem(hwnd, IDC_View), &view_);
  CreateColumn(ViewHostStart, ViewHostEnd, ::GetDlgItem(hwnd, IDC_ViewHost),
               &view_host_);
  CreateColumn(PluginStart, PluginEnd, ::GetDlgItem(hwnd, IDC_Plugin), &plugin_);
  CreateColumn(PluginHostStart, PluginHostEnd,
               ::GetDlgItem(hwnd, IDC_PluginHost), &plugin_host_);
  CreateColumn(NPObjectStart, NPObjectEnd, ::GetDlgItem(hwnd, IDC_NPObject),
               &npobject_);
  CreateColumn(PluginProcessStart, PluginProcessEnd,
               ::GetDlgItem(hwnd, IDC_PluginProcess), &plugin_process_);
  CreateColumn(PluginProcessHostStart, PluginProcessHostEnd,
               ::GetDlgItem(hwnd, IDC_PluginProcessHost), &plugin_process_host_);
  init_done_ = true;
}

void IPCStatusView::CreateColumn(
    uint16 start, uint16 end, HWND hwnd, CListViewCtrl** control) {
  DCHECK(*control == NULL);
  *control = new CListViewCtrl(hwnd);
  CListViewCtrl* control_ptr = *control;
  control_ptr->SetViewType(LVS_REPORT);
  control_ptr->SetExtendedListViewStyle(LVS_EX_CHECKBOXES);
  control_ptr->ModifyStyle(0, LVS_SORTASCENDING | LVS_NOCOLUMNHEADER);
  control_ptr->InsertColumn(0, L"id", LVCFMT_LEFT, 230);

  std::set<int>* disabled_messages = &current()->disabled_messages_;
  for (uint16 i = start; i < end; i++) {
    std::wstring name;
    IPC::Logging::GetMessageText(i, &name, NULL, NULL);

    int index = control_ptr->InsertItem(
        LVIF_TEXT | LVIF_PARAM, 0, name.c_str(), 0, 0, 0, i);

    control_ptr->SetItemText(index, 0, name.c_str());

    if (disabled_messages->find(i) == disabled_messages->end())
      control_ptr->SetCheckState(index, TRUE);
  }
}

void IPCStatusView::CloseDialog() {
  delete view_;
  delete view_host_;
  delete plugin_host_;
  delete npobject_;
  delete plugin_process_;
  delete plugin_process_host_;
  view_ = NULL;
  view_host_ = NULL;
  plugin_ = NULL;
  plugin_host_ = NULL;
  npobject_ = NULL;
  plugin_process_ = NULL;
  plugin_process_host_ = NULL;
  init_done_ = false;

  ::DestroyWindow(settings_dialog_);
  settings_dialog_ = NULL;

  Profile* current_profile = profile();
  if (!current_profile)
    return;
  PrefService* prefs = current_profile->GetPrefs();
  if (!prefs->IsPrefRegistered(prefs::kIpcDisabledMessages))
    return;
  ListValue* list = prefs->GetMutableList(prefs::kIpcDisabledMessages);
  list->Clear();
  for (std::set<int>::const_iterator itr = disabled_messages_.begin();
       itr != disabled_messages_.end();
       ++itr) {
    list->Append(Value::CreateIntegerValue(*itr));
  }
}

void IPCStatusView::OnCheck(int id, bool checked) {
  if (!init_done_)
    return;

  if (checked) {
    disabled_messages_.erase(id);
  } else {
    disabled_messages_.insert(id);
  }
}

void IPCStatusView::OnButtonClick(int id) {
  switch(id) {
   case IDC_ViewAll:
    CheckButtons(view_, true);
    break;
   case IDC_ViewNone:
    CheckButtons(view_, false);
     break;
   case IDC_ViewHostAll:
    CheckButtons(view_host_, true);
    break;
   case IDC_ViewHostNone:
    CheckButtons(view_host_, false);
     break;
   case IDC_PluginAll:
    CheckButtons(plugin_, true);
    break;
   case IDC_PluginNone:
    CheckButtons(plugin_, false);
     break;
   case IDC_PluginHostAll:
    CheckButtons(plugin_host_, true);
     break;
   case IDC_PluginHostNone:
    CheckButtons(plugin_host_, false);
    break;
   case IDC_NPObjectAll:
    CheckButtons(npobject_, true);
     break;
   case IDC_NPObjectNone:
    CheckButtons(npobject_, false);
    break;
  }
}

void IPCStatusView::CheckButtons(CListViewCtrl* control, bool check) {
  int count = control->GetItemCount();
  for (int i = 0; i < count; ++i)
    control->SetCheckState(i, check);
}

void IPCStatusView::OnSettings(UINT code, int button_id, HWND hwnd) {
  if (settings_dialog_ != NULL)
    return;

  HINSTANCE module_handle = GetModuleHandle(chrome::kBrowserResourcesDll);

  settings_dialog_ = CreateDialog(module_handle,
                                  MAKEINTRESOURCE(IDD_IPC_SETTINGS),
                                  NULL,
                                  IPCStatusView::DialogProc);
  ::ShowWindow(settings_dialog_, SW_SHOW);
}

#endif // IPC_MESSAGE_LOG_ENABLED
