// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_REQUEST_DIALOG_DELEGATE_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_REQUEST_DIALOG_DELEGATE_H_

#include "base/basictypes.h"

#include "chrome/browser/download/download_request_manager.h"

// DownloadRequestDialogDelegate is the dialog implementation used to prompt the
// the user as to whether they want to allow multiple downloads.
// DownloadRequestDialogDelegate delegates the allow/cancel methods to the
// TabDownloadState.
//
// TabDownloadState does not directly act as a dialog delegate because
// the dialog may outlive the TabDownloadState object.
class DownloadRequestDialogDelegate {
 public:
  // This factory method constructs a DownloadRequestDialogDelegate in a
  // platform-specific way.
  static DownloadRequestDialogDelegate* Create(TabContents* tab,
      DownloadRequestManager::TabDownloadState* host);

  void set_host(DownloadRequestManager::TabDownloadState* host) {
    host_ = host;
  }

  // Closes the prompt.
  virtual void CloseWindow() = 0;

 protected:
  explicit DownloadRequestDialogDelegate(
      DownloadRequestManager::TabDownloadState* host) : host_(host) { }

  virtual ~DownloadRequestDialogDelegate() { }

  virtual bool DoCancel() {
    if (host_)
      host_->Cancel();
    return true;
  }

  virtual bool DoAccept() {
    if (host_)
      host_->Accept();
    return true;
  }

  // The TabDownloadState we're displaying the dialog for. May be null.
  DownloadRequestManager::TabDownloadState* host_;

  DISALLOW_COPY_AND_ASSIGN(DownloadRequestDialogDelegate);
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_REQUEST_DIALOG_DELEGATE_H_

