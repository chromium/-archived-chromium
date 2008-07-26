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

#ifndef CHROME_BROWSER_JSMESSAGE_BOX_HANDLER_H__
#define CHROME_BROWSER_JSMESSAGE_BOX_HANDLER_H__

#include "chrome/common/ipc_message.h"
#include "chrome/views/app_modal_dialog_delegate.h"
#include "chrome/common/notification_service.h"

class MessageBoxView;
class WebContents;

namespace ChromeViews {
class Window;
}

class JavascriptMessageBoxHandler
    : public ChromeViews::AppModalDialogDelegate,
      public NotificationObserver {
 public:
  // Creates and runs a Javascript Message Box dialog.
  // The dialog type is specified within |dialog_flags|, the
  // default static display text is in |message_text| and if the dialog box is
  // a user input prompt() box, the default text for the text field is in
  // |default_prompt_text|. The result of the operation is returned using
  // |reply_msg|.
  static void RunJavascriptMessageBox(WebContents* web_contents,
                                      int dialog_flags,
                                      const std::wstring& message_text,
                                      const std::wstring& default_prompt_text,
                                      bool display_suppress_checkbox,
                                      IPC::Message* reply_msg);
  virtual ~JavascriptMessageBoxHandler();

  // ChromeViews::DialogDelegate Methods:
  virtual int GetDialogButtons() const;
  virtual std::wstring GetWindowTitle() const;
  virtual void WindowClosing();
  virtual bool Cancel();
  virtual bool Accept();

  // ChromeViews::AppModalDialogDelegate
  virtual void ShowModalDialog();
  virtual void ActivateModalDialog();

  // ChromeViews::WindowDelegate Methods:
  virtual bool IsModal() const { return true; }

 protected:
  // Use RunJavaScriptMessageBox to use.
  JavascriptMessageBoxHandler(WebContents* web_contents,
                              int dialog_flags,
                              const std::wstring& message_text,
                              const std::wstring& default_prompt_text,
                              bool display_suppress_checkbox,
                              IPC::Message* reply_msg);

 private:
  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // The message box view whose commands we handle.
  MessageBoxView* message_box_view_;

  // The IPC message used to reply to the renderer when the message box
  // is dismissed.
  IPC::Message* reply_msg_;

  // The associated WebContents. Used to send IPC messages to the renderer.
  WebContents* web_contents_;

  // Stores flags defined in message_box_view.h which describe the dialog box.
  int dialog_flags_;

  // The dialog if it is currently visible.
  ChromeViews::Window* dialog_;

  DISALLOW_EVIL_CONSTRUCTORS(JavascriptMessageBoxHandler);
};

#endif // CHROME_BROWSER_JSMESSAGE_BOX_HANDLER_H__
