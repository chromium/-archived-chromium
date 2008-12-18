// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_JSMESSAGE_BOX_HANDLER_H__
#define CHROME_BROWSER_JSMESSAGE_BOX_HANDLER_H__

#include "chrome/common/ipc_message.h"
#include "chrome/views/app_modal_dialog_delegate.h"
#include "chrome/common/notification_service.h"

class MessageBoxView;
class WebContents;
namespace views {
class Window;
}

class JavascriptMessageBoxHandler
    : public views::AppModalDialogDelegate,
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

  // views::DialogDelegate Methods:
  virtual int GetDialogButtons() const;
  virtual std::wstring GetWindowTitle() const;
  virtual void WindowClosing();
  virtual bool Cancel();
  virtual bool Accept();

  // views::AppModalDialogDelegate
  virtual void ShowModalDialog();
  virtual void ActivateModalDialog();

  // views::WindowDelegate Methods:
  virtual bool IsModal() const { return true; }
  virtual views::View* GetContentsView();
  virtual views::View* GetInitiallyFocusedView() const;

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
  views::Window* dialog_;

  DISALLOW_EVIL_CONSTRUCTORS(JavascriptMessageBoxHandler);
};

#endif // CHROME_BROWSER_JSMESSAGE_BOX_HANDLER_H__

