// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_REQUEST_DIALOG_DELEGATE_WIN_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_REQUEST_DIALOG_DELEGATE_WIN_H_

#include "base/basictypes.h"

#include "chrome/browser/download/download_request_dialog_delegate.h"
#include "chrome/browser/download/download_request_manager.h"
#include "views/window/dialog_delegate.h"

class ConstrainedWindow;
class MessageBoxView;
class TabContents;

class DownloadRequestDialogDelegateWin : public DownloadRequestDialogDelegate,
                                         public views::DialogDelegate {
 public:
  DownloadRequestDialogDelegateWin(TabContents* tab,
      DownloadRequestManager::TabDownloadState* host);

  void set_host(DownloadRequestManager::TabDownloadState* host) {
    host_ = host;
  }

 private:
  // DownloadRequestDialogDelegate methods.
  virtual void CloseWindow();
  // DialogDelegate methods.
  virtual bool Cancel();
  virtual bool Accept();
  virtual views::View* GetContentsView();
  virtual std::wstring GetDialogButtonLabel(
      MessageBoxFlags::DialogButton button) const;
  virtual int GetDefaultDialogButton() const {
    return MessageBoxFlags::DIALOGBUTTON_CANCEL;
  }
  virtual void DeleteDelegate();

  MessageBoxView* message_view_;

  ConstrainedWindow* window_;

  DISALLOW_COPY_AND_ASSIGN(DownloadRequestDialogDelegateWin);
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_REQUEST_DIALOG_DELEGATE_WIN_H_
