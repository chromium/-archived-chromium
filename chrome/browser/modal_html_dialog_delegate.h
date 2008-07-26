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

#ifndef CHROME_BROWSER_MODAL_HTML_DIALOG_DELEGATE_H__
#define CHROME_BROWSER_MODAL_HTML_DIALOG_DELEGATE_H__

#include <vector>

#include "chrome/browser/dom_ui/html_dialog_contents.h"
#include "chrome/common/notification_service.h"

// This class can only be used on the UI thread.
class ModalHtmlDialogDelegate
    : public HtmlDialogContentsDelegate,
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

  // ChromeViews::WindowDelegate implementation:
  virtual bool IsModal() const;

  // ModalHtmlDialogContents::ModalHTMLDialogContentsDelegate implementation:
  virtual GURL GetDialogContentURL() const;
  virtual void GetDialogSize(CSize* size) const;
  virtual std::string GetDialogArgs() const;
  virtual void OnDialogClosed(const std::string& json_retval);

 private:
  // The WebContents that opened the dialog.
  WebContents* contents_;

  // The parameters needed to display a modal HTML dialog.
  HtmlDialogContents::HtmlDialogParams params_;

  // Once we get our reply in OnModalDialogResponse we'll need to respond to the
  // plugin using this |sync_result| pointer so we store it between calls.
  IPC::Message* sync_response_;

  DISALLOW_EVIL_CONSTRUCTORS(ModalHtmlDialogDelegate);
};

#endif  // CHROME_BROWSER_MODAL_HTML_DIALOG_DELEGATE_H__
