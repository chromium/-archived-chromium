// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APP_MODAL_DIALOG_H_
#define CHROME_BROWSER_APP_MODAL_DIALOG_H_

#include <string>

#include "build/build_config.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"

#if defined(OS_WIN)
class JavascriptMessageBoxDialog;
typedef JavascriptMessageBoxDialog* NativeDialog;
#elif defined(OS_LINUX)
typedef struct _GtkWidget GtkWidget;
typedef GtkWidget* NativeDialog;
#elif defined(OS_MACOSX)
typedef void* NativeDialog;
#endif

class TabContents;
namespace IPC {
class Message;
}

// A controller+model class for javascript alert, confirm, prompt, and
// onbeforeunload dialog boxes.  |NativeDialog| is a platform specific
// view.
class AppModalDialog : public NotificationObserver {
 public:
  // A union of data necessary to determine the type of message box to
  // show.  |dialog_flags| is a MessageBox flag.
  AppModalDialog(TabContents* tab_contents,
                 const std::wstring& title,
                 int dialog_flags,
                 const std::wstring& message_text,
                 const std::wstring& default_prompt_text,
                 bool display_suppress_checkbox,
                 bool is_before_unload_dialog,
                 IPC::Message* reply_msg);
  ~AppModalDialog();

  // Called by the app modal window queue when it is time to show this window.
  void ShowModalDialog();

  /////////////////////////////////////////////////////////////////////////////
  // The following methods are platform specific and should be implemented in
  // the platform specific .cc files.
  // Create the platform specific NativeDialog and display it.  When the
  // NativeDialog is closed, it should call OnAccept or OnCancel to notify the
  // renderer of the user's action.  The NativeDialog is also expected to
  // delete the AppModalDialog associated with it.
  void CreateAndShowDialog();

  // Close the dialog if it is showing.
  void CloseModalDialog();

  // Called by the app modal window queue to activate the window.
  void ActivateModalDialog();

  /////////////////////////////////////////////////////////////////////////////
  // Getters so NativeDialog can get information about the message box.
  TabContents* tab_contents() {
    return tab_contents_;
  }
  int dialog_flags() {
    return dialog_flags_;
  }
  std::wstring title() {
    return title_;
  }
  bool is_before_unload_dialog() {
    return is_before_unload_dialog_;
  }

  // Callbacks from NativeDialog when the user accepts or cancels the dialog.
  void OnCancel();
  void OnAccept(const std::wstring& prompt_text, bool suppress_js_messages);
  void OnClose();

  // Helper methods used to query or control the dialog. This is used by
  // automation.
  int GetDialogButtons();
  void AcceptWindow();
  void CancelWindow();

 private:
  void InitNotifications();

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Sends APP_MODAL_DIALOG_CLOSED notification.
  void SendCloseNotification();

  NotificationRegistrar registrar_;

  // A reference to the platform native dialog box.
  NativeDialog dialog_;

  // Information about the message box is held in the following variables.
  TabContents* tab_contents_;
  std::wstring title_;
  int dialog_flags_;
  std::wstring message_text_;
  std::wstring default_prompt_text_;
  bool display_suppress_checkbox_;
  bool is_before_unload_dialog_;
  IPC::Message* reply_msg_;
};

#endif  // #ifndef CHROME_BROWSER_APP_MODAL_DIALOG_H_
