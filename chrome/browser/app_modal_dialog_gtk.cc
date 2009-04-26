// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/app_modal_dialog.h"

#include <gtk/gtk.h>

#include "base/logging.h"
#include "base/string_util.h"
#include "chrome/browser/tab_contents/web_contents.h"
#include "chrome/browser/tab_contents/tab_contents_view.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/message_box_flags.h"
#include "grit/generated_resources.h"

namespace {

void OnDialogResponse(GtkDialog* dialog, gint response_id,
                      AppModalDialog* app_modal_dialog) {
  switch (response_id) {
    case GTK_RESPONSE_OK:
      // The first arg is the prompt text and the second is true if we want to
      // suppress additional popups from the page.
      app_modal_dialog->OnAccept(std::wstring(), false);
      break;

    case GTK_RESPONSE_CANCEL:
    case GTK_RESPONSE_DELETE_EVENT: // User hit the X on the dialog.
      app_modal_dialog->OnCancel();
      break;

    default:
      NOTREACHED();
  }
  gtk_widget_destroy(GTK_WIDGET(dialog));
  delete app_modal_dialog;
}

}  // namespace

AppModalDialog::~AppModalDialog() {
}

void AppModalDialog::CreateAndShowDialog() {
  GtkButtonsType buttons = GTK_BUTTONS_NONE;
  GtkMessageType message_type = GTK_MESSAGE_OTHER;
  switch (dialog_flags_) {
    case MessageBoxFlags::kIsJavascriptAlert:
      buttons = GTK_BUTTONS_OK;
      message_type = GTK_MESSAGE_WARNING;
      break;

    case MessageBoxFlags::kIsJavascriptConfirm:
      if (is_before_unload_dialog_) {
        // onbeforeunload also uses a confirm prompt, it just has custom
        // buttons.  We add the buttons using gtk_dialog_add_button below.
        buttons = GTK_BUTTONS_NONE;
      } else {
        buttons = GTK_BUTTONS_OK_CANCEL;
      }
      message_type = GTK_MESSAGE_QUESTION;
      break;

    case MessageBoxFlags::kIsJavascriptPrompt:
      // We need to make a custom message box for javascript prompts. For now
      // just have an OK button and send back an empty string. Maybe we can
      // cram a GtkEntry into the content area of the message box via
      // gtk_dialog_get_content_area.
      // http://crbug.com/9623
      NOTIMPLEMENTED();
      buttons = GTK_BUTTONS_OK;
      message_type = GTK_MESSAGE_QUESTION;
      break;

    default:
      NOTREACHED();
  }

  GtkWindow* window = web_contents_->view()->GetTopLevelNativeWindow();
  dialog_ = gtk_message_dialog_new(window, GTK_DIALOG_MODAL,
      message_type, buttons, "%s", WideToUTF8(message_text_).c_str());
  gtk_window_set_title(GTK_WINDOW(dialog_), WideToUTF8(title_).c_str());

  if (is_before_unload_dialog_) {
    std::string button_text = l10n_util::GetStringUTF8(
        IDS_BEFOREUNLOAD_MESSAGEBOX_OK_BUTTON_LABEL);
    gtk_dialog_add_button(GTK_DIALOG(dialog_), button_text.c_str(),
                          GTK_RESPONSE_OK);

    button_text = l10n_util::GetStringUTF8(
        IDS_BEFOREUNLOAD_MESSAGEBOX_CANCEL_BUTTON_LABEL);
    gtk_dialog_add_button(GTK_DIALOG(dialog_), button_text.c_str(),
                          GTK_RESPONSE_CANCEL);
  }

  g_signal_connect(dialog_, "response", G_CALLBACK(OnDialogResponse), this);
  gtk_widget_show(GTK_WIDGET(GTK_DIALOG(dialog_)));
}

void AppModalDialog::ActivateModalDialog() {
  NOTIMPLEMENTED();
}

void AppModalDialog::CloseModalDialog() {
  NOTIMPLEMENTED();
}

int AppModalDialog::GetDialogButtons() {
  switch (dialog_flags_) {
    case MessageBoxFlags::kIsJavascriptAlert:
      return MessageBoxFlags::DIALOGBUTTON_OK;

    case MessageBoxFlags::kIsJavascriptConfirm:
      return MessageBoxFlags::DIALOGBUTTON_OK |
             MessageBoxFlags::DIALOGBUTTON_CANCEL;

    case MessageBoxFlags::kIsJavascriptPrompt:
      return MessageBoxFlags::DIALOGBUTTON_OK;

    default:
      NOTREACHED();
      return 0;
  }
}

void AppModalDialog::AcceptWindow() {
  OnDialogResponse(GTK_DIALOG(dialog_), GTK_RESPONSE_OK, this);
}

void AppModalDialog::CancelWindow() {
  OnDialogResponse(GTK_DIALOG(dialog_), GTK_RESPONSE_CANCEL, this);
}
