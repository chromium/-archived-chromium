// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/app_modal_dialog.h"

#include <gtk/gtk.h>

#include "app/l10n_util.h"
#include "app/message_box_flags.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_view.h"
#include "grit/generated_resources.h"

namespace {

// If there's a text entry in the dialog, get the text from the first one and
// return it.
std::wstring GetPromptText(GtkDialog* dialog) {
  std::wstring text;
  // TODO(tc): Replace with gtk_dialog_get_content_area() when using GTK 2.14+
  GtkWidget* contents_vbox = dialog->vbox;
  GList* first_child = gtk_container_get_children(GTK_CONTAINER(contents_vbox));
  for (GList* item = first_child; item; item = g_list_next(item)) {
    if (GTK_IS_ENTRY(item->data)) {
      text = UTF8ToWide(gtk_entry_get_text(GTK_ENTRY(item->data)));
    }
  }
  g_list_free(first_child);
  return text;
}

void OnDialogResponse(GtkDialog* dialog, gint response_id,
                      AppModalDialog* app_modal_dialog) {
  switch (response_id) {
    case GTK_RESPONSE_OK:
      // The first arg is the prompt text and the second is true if we want to
      // suppress additional popups from the page.
      app_modal_dialog->OnAccept(GetPromptText(dialog), false);
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
      buttons = GTK_BUTTONS_OK_CANCEL;
      message_type = GTK_MESSAGE_QUESTION;
      break;

    default:
      NOTREACHED();
  }

  GtkWindow* window = tab_contents_->view()->GetTopLevelNativeWindow();
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
  } else if (MessageBoxFlags::kIsJavascriptPrompt == dialog_flags_) {
    // TODO(tc): Replace with gtk_dialog_get_content_area() when using GTK 2.14+
    GtkWidget* contents_vbox = GTK_DIALOG(dialog_)->vbox;
    GtkWidget* text_box = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(text_box),
                       WideToUTF8(default_prompt_text_).c_str());
    gtk_widget_show(text_box);
    gtk_box_pack_start(GTK_BOX(contents_vbox), text_box, TRUE, TRUE, 0);
  }

  g_signal_connect(dialog_, "response", G_CALLBACK(OnDialogResponse), this);
  gtk_widget_show(GTK_WIDGET(GTK_DIALOG(dialog_)));
}

void AppModalDialog::ActivateModalDialog() {
  gtk_window_present(GTK_WINDOW(dialog_));
}

void AppModalDialog::CloseModalDialog() {
  OnDialogResponse(GTK_DIALOG(dialog_), GTK_RESPONSE_DELETE_EVENT, this);
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
