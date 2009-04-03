// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MODAL_HTML_DIALOG_DELEGATE_H_
#define CHROME_BROWSER_MODAL_HTML_DIALOG_DELEGATE_H_

#include <vector>

#include "base/gfx/size.h"
#include "chrome/browser/dom_ui/html_dialog_ui.h"
#include "chrome/common/ipc_message.h"
#include "chrome/common/notification_observer.h"

// This class can only be used on the UI thread.
class ModalHtmlDialogDelegate
    : public HtmlDialogUIDelegate,
      public NotificationObserver {
 public:
  ModalHtmlDialogDelegate(const GURL& url,
                          int width, int height,
                          const std::string& json_arguments,
                          IPC::Message* sync_result,
                          WebContents* contents);
  ~ModalHtmlDialogDelegate();

  // Notification service callback.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // HTMLDialogUIDelegate implementation:
  virtual bool IsDialogModal() const;
  virtual std::wstring GetDialogTitle() const { return L"Google Gears"; }
  virtual GURL GetDialogContentURL() const;
  virtual void GetDialogSize(gfx::Size* size) const;
  virtual std::string GetDialogArgs() const;
  virtual void OnDialogClosed(const std::string& json_retval);

 private:
  // Invoked from the destructor or when we receive notification the web
  // contents has been disconnnected. Removes the observer from the WebContents
  // and NULLs out contents_.
  void RemoveObserver();

  // The WebContents that opened the dialog.
  WebContents* contents_;

  // The parameters needed to display a modal HTML dialog.
  HtmlDialogUI::HtmlDialogParams params_;

  // Once we get our reply in OnModalDialogResponse we'll need to respond to the
  // plugin using this |sync_result| pointer so we store it between calls.
  IPC::Message* sync_response_;

  DISALLOW_COPY_AND_ASSIGN(ModalHtmlDialogDelegate);
};

#endif  // CHROME_BROWSER_MODAL_HTML_DIALOG_DELEGATE_H_
