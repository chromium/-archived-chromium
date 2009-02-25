// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_JSMESSAGE_BOX_HANDLER_WIN_H_
#define CHROME_BROWSER_JSMESSAGE_BOX_HANDLER_WIN_H_

#include "chrome/browser/app_modal_dialog_delegate.h"
#include "chrome/browser/jsmessage_box_handler.h"
#include "chrome/common/ipc_message.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/views/dialog_delegate.h"

class MessageBoxView;
class WebContents;
namespace views {
class Window;
}

class JavascriptMessageBoxHandler
    : public AppModalDialogDelegate,
      public AppModalDialogDelegateTesting,
      public NotificationObserver,
      public views::DialogDelegate {
 public:
  // Cross-platform code should use RunJavaScriptMessageBox.
  JavascriptMessageBoxHandler(WebContents* web_contents,
                              int dialog_flags,
                              const std::wstring& message_text,
                              const std::wstring& default_prompt_text,
                              bool display_suppress_checkbox,
                              IPC::Message* reply_msg);
  virtual ~JavascriptMessageBoxHandler();

  // AppModalDialogDelegate Methods:
  virtual void ShowModalDialog();
  virtual void ActivateModalDialog();
  virtual AppModalDialogDelegateTesting* GetTestingInterface();

  // AppModalDialogDelegateTesting Methods:
  virtual views::DialogDelegate* GetDialogDelegate();

  // views::DialogDelegate Methods:
  virtual int GetDialogButtons() const;
  virtual std::wstring GetWindowTitle() const;
  virtual void WindowClosing();
  virtual bool Cancel();
  virtual bool Accept();

  // views::WindowDelegate Methods:
  virtual bool IsModal() const { return true; }
  virtual views::View* GetContentsView();
  virtual views::View* GetInitiallyFocusedView();

 private:
  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  NotificationRegistrar registrar_;

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

  DISALLOW_COPY_AND_ASSIGN(JavascriptMessageBoxHandler);
};

#endif  // CHROME_BROWSER_JSMESSAGE_BOX_HANDLER_WIN_H_
