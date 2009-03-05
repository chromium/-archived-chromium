// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/default_plugin/install_dialog.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "grit/webkit_strings.h"
#include "webkit/default_plugin/plugin_impl.h"
#include "webkit/glue/webkit_glue.h"

bool PluginInstallDialog::Initialize(PluginInstallerImpl* plugin_impl,
                                     const std::wstring& plugin_name) {
  if (!plugin_impl) {
    NOTREACHED();
    return false;
  }

  plugin_impl_ = plugin_impl;
  plugin_name_ = plugin_name;
  return true;
}

HWND PluginInstallDialog::Create(HWND parent_window, LPARAM init_param) {
  // Most of the code here is based on CDialogImpl<T>::Create.
  DCHECK(m_hWnd == NULL);

  // Allocate the thunk structure here, where we can fail
  // gracefully.
  BOOL thunk_inited = m_thunk.Init(NULL, NULL);
  if (thunk_inited == FALSE) {
    SetLastError(ERROR_OUTOFMEMORY);
    return NULL;
  }

  _AtlWinModule.AddCreateWndData(&m_thunk.cd, this);

#ifdef _DEBUG
  m_bModal = false;
#endif // _DEBUG

  HINSTANCE instance_handle = _AtlBaseModule.GetResourceInstance();

  HRSRC dialog_resource = FindResource(instance_handle,
                                       MAKEINTRESOURCE(IDD), RT_DIALOG);
  if (!dialog_resource) {
    NOTREACHED();
    return NULL;
  }

  HGLOBAL dialog_template = LoadResource(instance_handle,
                                         dialog_resource);
  if (!dialog_template) {
    NOTREACHED();
    return NULL;
  }

  _DialogSplitHelper::DLGTEMPLATEEX* dialog_template_struct =
      reinterpret_cast<_DialogSplitHelper::DLGTEMPLATEEX *>(
          ::LockResource(dialog_template));
  DCHECK(dialog_template_struct != NULL);

  unsigned long dialog_template_size =
      SizeofResource(instance_handle, dialog_resource);

  HGLOBAL rtl_layout_dialog_template = NULL;

  if (IsRTLLayout()) {
    rtl_layout_dialog_template = GlobalAlloc(GPTR, dialog_template_size);
    DCHECK(rtl_layout_dialog_template != NULL);

    _DialogSplitHelper::DLGTEMPLATEEX* rtl_layout_dialog_template_struct =
        reinterpret_cast<_DialogSplitHelper::DLGTEMPLATEEX*>(
            ::GlobalLock(rtl_layout_dialog_template));
    DCHECK(rtl_layout_dialog_template_struct != NULL);

    memcpy(rtl_layout_dialog_template_struct, dialog_template_struct,
           dialog_template_size);

    rtl_layout_dialog_template_struct->exStyle |=
        (WS_EX_LAYOUTRTL | WS_EX_RTLREADING);

    dialog_template_struct = rtl_layout_dialog_template_struct;
  }

  HWND dialog_window =
      CreateDialogIndirectParam(
          instance_handle,
          reinterpret_cast<DLGTEMPLATE*>(dialog_template_struct),
          parent_window, StartDialogProc, init_param);

  DCHECK(m_hWnd == dialog_window);

  if (rtl_layout_dialog_template) {
    GlobalUnlock(rtl_layout_dialog_template);
    GlobalFree(rtl_layout_dialog_template);
  }

  return dialog_window;
}


LRESULT PluginInstallDialog::OnInitDialog(UINT message, WPARAM wparam,
                                          LPARAM lparam, BOOL& handled) {
  std::wstring dialog_title =
      PluginInstallerImpl::ReplaceStringForPossibleEmptyReplacement(
          IDS_DEFAULT_PLUGIN_CONFIRMATION_DIALOG_TITLE,
          IDS_DEFAULT_PLUGIN_CONFIRMATION_DIALOG_TITLE_NO_PLUGIN_NAME,
          plugin_name_);
  AdjustTextDirectionality(&dialog_title);
  SetWindowText(dialog_title.c_str());

  std::wstring get_the_plugin_btn_msg =
      webkit_glue::GetLocalizedString(
          IDS_DEFAULT_PLUGIN_GET_THE_PLUGIN_BTN_MSG);
  AdjustTextDirectionality(&get_the_plugin_btn_msg);
  SetDlgItemText(IDB_GET_THE_PLUGIN, get_the_plugin_btn_msg.c_str());

  std::wstring cancel_plugin_download_msg =
      webkit_glue::GetLocalizedString(
          IDS_DEFAULT_PLUGIN_CANCEL_PLUGIN_DOWNLOAD_MSG);
  AdjustTextDirectionality(&cancel_plugin_download_msg);
  SetDlgItemText(IDCANCEL, cancel_plugin_download_msg.c_str());

  std::wstring plugin_user_action_msg =
      PluginInstallerImpl::ReplaceStringForPossibleEmptyReplacement(
          IDS_DEFAULT_PLUGIN_USER_OPTION_MSG,
          IDS_DEFAULT_PLUGIN_USER_OPTION_MSG_NO_PLUGIN_NAME,
          plugin_name_);
  AdjustTextDirectionality(&plugin_user_action_msg);
  SetDlgItemText(IDC_PLUGIN_INSTALL_CONFIRMATION_LABEL,
                 plugin_user_action_msg.c_str());
  return 0;
}


LRESULT PluginInstallDialog::OnGetPlugin(WORD notify_code, WORD id,
                                         HWND wnd_ctl, BOOL &handled) {
  if (!plugin_impl_) {
    NOTREACHED();
    return 0;
  }

  DestroyWindow();
  plugin_impl_->DownloadPlugin();
  return 0;
}

LRESULT PluginInstallDialog::OnCancel(WORD notify_code, WORD id, HWND wnd_ctl,
                                      BOOL &handled) {
  DestroyWindow();
  plugin_impl_->DownloadCancelled();
  return 0;
}


bool PluginInstallDialog::IsRTLLayout() const {
  return plugin_impl_ ? plugin_impl_->IsRTLLayout() : false;
}

// TODO(idana) bug# 1246452: use the library l10n_util once it is moved from
// the Chrome module into the Base module. For now, we simply copy/paste the
// same code.
void PluginInstallDialog::AdjustTextDirectionality(std::wstring* text) const {
  if (IsRTLLayout()) {
    // Inserting an RLE (Right-To-Left Embedding) mark as the first character.
    text->insert(0, L"\x202B");

    // Inserting a PDF (Pop Directional Formatting) mark as the last character.
    text->append(L"\x202C");
  }
}

