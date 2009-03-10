// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_request_dialog_delegate_win.h"

#include "chrome/browser/tab_contents/constrained_window.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/l10n_util.h"
#include "chrome/views/message_box_view.h"
#include "grit/generated_resources.h"

// static
DownloadRequestDialogDelegate* DownloadRequestDialogDelegate::Create(
    TabContents* tab,
    DownloadRequestManager::TabDownloadState* host) {
  return new DownloadRequestDialogDelegateWin(tab, host);
}

DownloadRequestDialogDelegateWin::DownloadRequestDialogDelegateWin(
    TabContents* tab,
    DownloadRequestManager::TabDownloadState* host)
    : DownloadRequestDialogDelegate(host) {
  message_view_ = new MessageBoxView(
      MessageBoxView::kIsConfirmMessageBox,
      l10n_util::GetString(IDS_MULTI_DOWNLOAD_WARNING),
      std::wstring());
  window_ = tab->CreateConstrainedDialog(this, message_view_);
}

void DownloadRequestDialogDelegateWin::CloseWindow() {
  window_->CloseConstrainedWindow();
}

bool DownloadRequestDialogDelegateWin::Cancel() {
  return DoCancel();
}

bool DownloadRequestDialogDelegateWin::Accept() {
  return DoAccept();
}

views::View* DownloadRequestDialogDelegateWin::GetContentsView() {
  return message_view_;
}

std::wstring DownloadRequestDialogDelegateWin::GetDialogButtonLabel(
    DialogButton button) const {
  if (button == DIALOGBUTTON_OK)
    return l10n_util::GetString(IDS_MULTI_DOWNLOAD_WARNING_ALLOW);
  if (button == DIALOGBUTTON_CANCEL)
    return l10n_util::GetString(IDS_MULTI_DOWNLOAD_WARNING_DENY);
  return std::wstring();
}

void DownloadRequestDialogDelegateWin::DeleteDelegate() {
  DCHECK(!host_);
  delete this;
}
