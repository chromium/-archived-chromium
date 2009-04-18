// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/app_modal_dialog.h"

#include "chrome/browser/app_modal_dialog_queue.h"
#include "chrome/browser/tab_contents/web_contents.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/ipc_message.h"

AppModalDialog::AppModalDialog(WebContents* web_contents,
                               const std::wstring& title,
                               int dialog_flags,
                               const std::wstring& message_text,
                               const std::wstring& default_prompt_text,
                               bool display_suppress_checkbox,
                               bool is_before_unload_dialog,
                               IPC::Message* reply_msg)
    : web_contents_(web_contents),
      title_(title),
      dialog_flags_(dialog_flags),
      message_text_(message_text),
      default_prompt_text_(default_prompt_text),
      display_suppress_checkbox_(display_suppress_checkbox),
      is_before_unload_dialog_(is_before_unload_dialog),
      reply_msg_(reply_msg) {
  InitNotifications();
}

void AppModalDialog::Observe(NotificationType type,
                             const NotificationSource& source,
                             const NotificationDetails& details) {
  if (!web_contents_)
    return;

  if (type == NotificationType::NAV_ENTRY_COMMITTED &&
      Source<NavigationController>(source).ptr() ==
          &web_contents_->controller())
    web_contents_ = NULL;

  if (type == NotificationType::TAB_CONTENTS_DESTROYED &&
      Source<TabContents>(source).ptr() ==
      static_cast<TabContents*>(web_contents_))
    web_contents_ = NULL;

  if (!web_contents_)
    CloseModalDialog();
}

void AppModalDialog::InitNotifications() {
  // Make sure we get navigation notifications so we know when our parent
  // contents will disappear or navigate to a different page.
  registrar_.Add(this, NotificationType::NAV_ENTRY_COMMITTED,
                 NotificationService::AllSources());
  registrar_.Add(this, NotificationType::TAB_CONTENTS_DESTROYED,
                 NotificationService::AllSources());
}

void AppModalDialog::ShowModalDialog() {
  // If the WebContents that created this dialog navigated away before this
  // dialog became visible, simply show the next dialog if any.
  if (!web_contents_) {
    AppModalDialogQueue::ShowNextDialog();
    delete this;
    return;
  }

  web_contents_->Activate();
  CreateAndShowDialog();
}

void AppModalDialog::OnCancel() {
  // We need to do this before WM_DESTROY (WindowClosing()) as any parent frame
  // will receive it's activation messages before this dialog receives
  // WM_DESTROY. The parent frame would then try to activate any modal dialogs
  // that were still open in the ModalDialogQueue, which would send activation
  // back to this one. The framework should be improved to handle this, so this
  // is a temporary workaround.
  AppModalDialogQueue::ShowNextDialog();

  if (web_contents_) {
    web_contents_->OnJavaScriptMessageBoxClosed(reply_msg_, false,
                                                std::wstring());
  }
}

void AppModalDialog::OnAccept(const std::wstring& prompt_text,
                              bool suppress_js_messages) {
  AppModalDialogQueue::ShowNextDialog();

  if (web_contents_) {
    web_contents_->OnJavaScriptMessageBoxClosed(reply_msg_, true,
                                                prompt_text);

    if (suppress_js_messages)
      web_contents()->set_suppress_javascript_messages(true);
  }
}
