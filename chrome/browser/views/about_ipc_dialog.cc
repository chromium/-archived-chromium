// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Need to include this before any other file because it defines
// IPC_MESSAGE_LOG_ENABLED. We need to use it to define
// IPC_MESSAGE_MACROS_LOG_ENABLED so render_messages.h will generate the
// ViewMsgLog et al. functions.
#include "chrome/common/ipc_message.h"

#ifdef IPC_MESSAGE_LOG_ENABLED
#define IPC_MESSAGE_MACROS_LOG_ENABLED

#include "chrome/browser/views/about_ipc_dialog.h"

#include <set>

#include "base/string_util.h"
#include "base/thread.h"
#include "base/time.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/views/standard_layout.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/plugin_messages.h"
#include "chrome/common/render_messages.h"
#include "chrome/views/grid_layout.h"
#include "chrome/views/text_button.h"
#include "chrome/views/window.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_job_tracker.h"
#include "chrome/views/hwnd_view.h"
#include "chrome/views/root_view.h"

namespace {

// We don't localize this UI since this is a developer-only feature.
const wchar_t kStartTrackingLabel[] = L"Start tracking";
const wchar_t kStopTrackingLabel[] = L"Stop tracking";
const wchar_t kClearLabel[] = L"Clear";
const wchar_t kFilterLabel[] = L"Filter...";

enum {
  kTimeColumn = 0,
  kChannelColumn,
  kMessageColumn,
  kFlagsColumn,
  kDispatchColumn,
  kProcessColumn,
  kParamsColumn,
};

// This class registers the browser IPC logger functions with IPC::Logging.
class RegisterLoggerFuncs {
 public:
  RegisterLoggerFuncs() {
    IPC::Logging::SetLoggerFunctions(g_log_function_mapping);
  }
};

RegisterLoggerFuncs g_register_logger_funcs;

// The singleton dialog box. This is non-NULL when a dialog is active so we
// know not to create a new one.
AboutIPCDialog* active_dialog = NULL;

std::set<int> disabled_messages;

// Settings dialog -------------------------------------------------------------

bool init_done = false;
HWND settings_dialog = NULL;

// Settings lists.
struct Settings {
  CListViewCtrl* view;
  CListViewCtrl* view_host;
  CListViewCtrl* plugin;
  CListViewCtrl* plugin_host;
  CListViewCtrl* npobject;
  CListViewCtrl* plugin_process;
  CListViewCtrl* plugin_process_host;
} settings_views = { NULL, NULL, NULL, NULL, NULL, NULL, NULL };

void CreateColumn(uint16 start, uint16 end, HWND hwnd,
                  CListViewCtrl** control) {
  DCHECK(*control == NULL);
  *control = new CListViewCtrl(hwnd);
  CListViewCtrl* control_ptr = *control;
  control_ptr->SetViewType(LVS_REPORT);
  control_ptr->SetExtendedListViewStyle(LVS_EX_CHECKBOXES);
  control_ptr->ModifyStyle(0, LVS_SORTASCENDING | LVS_NOCOLUMNHEADER);
  control_ptr->InsertColumn(0, L"id", LVCFMT_LEFT, 230);

  for (uint16 i = start; i < end; i++) {
    std::wstring name;
    IPC::Logging::GetMessageText(i, &name, NULL, NULL);

    int index = control_ptr->InsertItem(
        LVIF_TEXT | LVIF_PARAM, 0, name.c_str(), 0, 0, 0, i);

    control_ptr->SetItemText(index, 0, name.c_str());

    if (disabled_messages.find(i) == disabled_messages.end())
      control_ptr->SetCheckState(index, TRUE);
  }
}

void OnCheck(int id, bool checked) {
  if (!init_done)
    return;

  if (checked)
    disabled_messages.erase(id);
  else
    disabled_messages.insert(id);
}


void CheckButtons(CListViewCtrl* control, bool check) {
  int count = control->GetItemCount();
  for (int i = 0; i < count; ++i)
    control->SetCheckState(i, check);
}

void InitDialog(HWND hwnd) {
  CreateColumn(ViewStart, ViewEnd, ::GetDlgItem(hwnd, IDC_View),
               &settings_views.view);
  CreateColumn(ViewHostStart, ViewHostEnd, ::GetDlgItem(hwnd, IDC_ViewHost),
               &settings_views.view_host);
  CreateColumn(PluginStart, PluginEnd, ::GetDlgItem(hwnd, IDC_Plugin),
               &settings_views.plugin);
  CreateColumn(PluginHostStart, PluginHostEnd,
               ::GetDlgItem(hwnd, IDC_PluginHost),
               &settings_views.plugin_host);
  CreateColumn(NPObjectStart, NPObjectEnd, ::GetDlgItem(hwnd, IDC_NPObject),
               &settings_views.npobject);
  CreateColumn(PluginProcessStart, PluginProcessEnd,
               ::GetDlgItem(hwnd, IDC_PluginProcess),
               &settings_views.plugin_process);
  CreateColumn(PluginProcessHostStart, PluginProcessHostEnd,
               ::GetDlgItem(hwnd, IDC_PluginProcessHost),
               &settings_views.plugin_process_host);
  init_done = true;
}

void CloseDialog() {
  delete settings_views.view;
  delete settings_views.view_host;
  delete settings_views.plugin_host;
  delete settings_views.npobject;
  delete settings_views.plugin_process;
  delete settings_views.plugin_process_host;
  settings_views.view = NULL;
  settings_views.view_host = NULL;
  settings_views.plugin = NULL;
  settings_views.plugin_host = NULL;
  settings_views.npobject = NULL;
  settings_views.plugin_process = NULL;
  settings_views.plugin_process_host = NULL;

  init_done = false;

  ::DestroyWindow(settings_dialog);
  settings_dialog = NULL;

  /* The old version of this code stored the last settings in the preferences.
     But with this dialog, there currently isn't an easy way to get the profile
     to asave in the preferences.
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
  */
}

void OnButtonClick(int id) {
  switch(id) {
    case IDC_ViewAll:
      CheckButtons(settings_views.view, true);
      break;
    case IDC_ViewNone:
      CheckButtons(settings_views.view, false);
      break;
    case IDC_ViewHostAll:
      CheckButtons(settings_views.view_host, true);
      break;
    case IDC_ViewHostNone:
      CheckButtons(settings_views.view_host, false);
      break;
    case IDC_PluginAll:
      CheckButtons(settings_views.plugin, true);
      break;
    case IDC_PluginNone:
      CheckButtons(settings_views.plugin, false);
      break;
    case IDC_PluginHostAll:
      CheckButtons(settings_views.plugin_host, true);
      break;
    case IDC_PluginHostNone:
      CheckButtons(settings_views.plugin_host, false);
      break;
    case IDC_NPObjectAll:
      CheckButtons(settings_views.npobject, true);
      break;
    case IDC_NPObjectNone:
      CheckButtons(settings_views.npobject, false);
      break;
  }
}

INT_PTR CALLBACK DialogProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
  switch (msg) {
    case WM_INITDIALOG:
      InitDialog(hwnd);
      return FALSE;  // Don't set keyboard focus.
    case WM_SYSCOMMAND:
      if (wparam == SC_CLOSE) {
        CloseDialog();
        return FALSE;
      }
      break;
    case WM_NOTIFY: {
      NMLISTVIEW* info = reinterpret_cast<NM_LISTVIEW*>(lparam);
      if ((wparam == IDC_View || wparam == IDC_ViewHost ||
           wparam == IDC_Plugin ||
           wparam == IDC_PluginHost || wparam == IDC_NPObject ||
           wparam == IDC_PluginProcess || wparam == IDC_PluginProcessHost) &&
          info->hdr.code == LVN_ITEMCHANGED) {
        if (info->uChanged & LVIF_STATE) {
          bool checked = (info->uNewState >> 12) == 2;
          OnCheck(static_cast<int>(info->lParam), checked);
        }
        return FALSE;
      }
      break;
    }
    case WM_COMMAND:
      if (HIWORD(wparam) == BN_CLICKED)
        OnButtonClick(LOWORD(wparam));
      break;
  }
  return FALSE;
}

void RunSettingsDialog(HWND parent) {
  if (settings_dialog)
    return;
  HINSTANCE module_handle = GetModuleHandle(chrome::kBrowserResourcesDll);
  settings_dialog = CreateDialog(module_handle,
                                 MAKEINTRESOURCE(IDD_IPC_SETTINGS),
                                 NULL,
                                 &DialogProc);
  ::ShowWindow(settings_dialog, SW_SHOW);
}

}  // namespace

// AboutIPCDialog --------------------------------------------------------------

AboutIPCDialog::AboutIPCDialog()
    : track_toggle_(NULL),
      clear_button_(NULL),
      filter_button_(NULL),
      table_(NULL),
      tracking_(false) {
  SetupControls();
  IPC::Logging::current()->SetConsumer(this);
}

AboutIPCDialog::~AboutIPCDialog() {
  active_dialog = NULL;
  IPC::Logging::current()->SetConsumer(NULL);
}

// static
void AboutIPCDialog::RunDialog() {
  if (!active_dialog) {
    active_dialog = new AboutIPCDialog;
    views::Window::CreateChromeWindow(NULL, gfx::Rect(), active_dialog)->Show();
  } else {
    // TOOD(brettw) it would be nice to focus the existing window.
  }
}

void AboutIPCDialog::SetupControls() {
  views::GridLayout* layout = CreatePanelGridLayout(this);
  SetLayoutManager(layout);

  track_toggle_ = new views::TextButton(kStartTrackingLabel);
  track_toggle_->SetListener(this, 1);
  clear_button_ = new views::TextButton(kClearLabel);
  clear_button_->SetListener(this, 2);
  filter_button_ = new views::TextButton(kFilterLabel);
  filter_button_->SetListener(this, 3);

  table_ = new views::HWNDView();

  static const int first_column_set = 1;
  views::ColumnSet* column_set = layout->AddColumnSet(first_column_set);
  column_set->AddColumn(views::GridLayout::CENTER, views::GridLayout::CENTER,
                        33.33f, views::GridLayout::FIXED, 0, 0);
  column_set->AddColumn(views::GridLayout::CENTER, views::GridLayout::CENTER,
                        33.33f, views::GridLayout::FIXED, 0, 0);
  column_set->AddColumn(views::GridLayout::CENTER, views::GridLayout::CENTER,
                        33.33f, views::GridLayout::FIXED, 0, 0);

  static const int table_column_set = 2;
  column_set = layout->AddColumnSet(table_column_set);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL,
                        100.0f, views::GridLayout::FIXED, 0, 0);

  layout->StartRow(0, first_column_set);
  layout->AddView(track_toggle_);
  layout->AddView(clear_button_);
  layout->AddView(filter_button_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(1.0f, table_column_set);
  layout->AddView(table_);
}

gfx::Size AboutIPCDialog::GetPreferredSize() {
  return gfx::Size(800, 400);
}

views::View* AboutIPCDialog::GetContentsView() {
  return this;
}

int AboutIPCDialog::GetDialogButtons() const {
  // Don't want OK or Cancel.
  return 0;
}

std::wstring AboutIPCDialog::GetWindowTitle() const {
  return L"about:ipc";
}

void AboutIPCDialog::Layout() {
  if (!message_list_.m_hWnd) {
    HWND parent_window = GetRootView()->GetWidget()->GetHWND();

    CRect rect(0, 0, 10, 10);
    HWND list_hwnd = message_list_.Create(parent_window,
        rect, NULL, WS_CHILD | WS_VISIBLE | LVS_SORTASCENDING);
    message_list_.SetViewType(LVS_REPORT);
    message_list_.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT);

    int column_index = 0;
    message_list_.InsertColumn(kTimeColumn, L"time", LVCFMT_LEFT, 80);
    message_list_.InsertColumn(kChannelColumn, L"channel", LVCFMT_LEFT, 110);
    message_list_.InsertColumn(kMessageColumn, L"message", LVCFMT_LEFT, 240);
    message_list_.InsertColumn(kFlagsColumn, L"flags", LVCFMT_LEFT, 50);
    message_list_.InsertColumn(kDispatchColumn, L"dispatch (ms)", LVCFMT_RIGHT,
                               80);
    message_list_.InsertColumn(kProcessColumn, L"process (ms)", LVCFMT_RIGHT,
                               80);
    message_list_.InsertColumn(kParamsColumn, L"parameters", LVCFMT_LEFT, 500);

    table_->Attach(list_hwnd);
  }

  View::Layout();
}

void AboutIPCDialog::Log(const IPC::LogData& data) {
  if (disabled_messages.find(data.type) != disabled_messages.end())
    return;  // Message type is filtered out.

  base::Time sent = base::Time::FromInternalValue(data.sent);
  base::Time::Exploded exploded;
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

  int64 time_to_send = (base::Time::FromInternalValue(data.receive) -
      sent).InMilliseconds();
  // time can go backwards by a few ms (see Time), don't show that.
  time_to_send = std::max(static_cast<int>(time_to_send), 0);
  std::wstring temp = StringPrintf(L"%d", time_to_send);
  message_list_.SetItemText(index, kDispatchColumn, temp.c_str());

  int64 time_to_process = (base::Time::FromInternalValue(data.dispatch) -
      base::Time::FromInternalValue(data.receive)).InMilliseconds();
  time_to_process = std::max(static_cast<int>(time_to_process), 0);
  temp = StringPrintf(L"%d", time_to_process);
  message_list_.SetItemText(index, kProcessColumn, temp.c_str());

  message_list_.SetItemText(index, kParamsColumn, data.params.c_str());
  message_list_.EnsureVisible(index, FALSE);
}

bool AboutIPCDialog::CanResize() const {
  return true;
}

void AboutIPCDialog::ButtonPressed(views::BaseButton* button) {
  if (button == track_toggle_) {
    if (tracking_) {
      track_toggle_->SetText(kStartTrackingLabel);
      tracking_ = false;
      IPC::Logging::current()->Disable();
    } else {
      track_toggle_->SetText(kStopTrackingLabel);
      tracking_ = true;
      IPC::Logging::current()->Enable();
    }
    track_toggle_->SchedulePaint();
  } else if (button == clear_button_) {
    message_list_.DeleteAllItems();
  } else if (button == filter_button_) {
    RunSettingsDialog(GetRootView()->GetWidget()->GetHWND());
  }
}

#endif  // IPC_MESSAGE_LOG_ENABLED
