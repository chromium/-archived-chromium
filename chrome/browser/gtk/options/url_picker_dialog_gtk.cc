// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtk/gtk.h>

#include "app/l10n_util.h"
#include "base/message_loop.h"
#include "chrome/browser/gtk/options/url_picker_dialog_gtk.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"

namespace {

// Horizontal spacing between a label and its control.
const int kLabelSpacing = 12;

// Space inside edges of a dialog.
const int kDialogSpacing = 18;

// Initial width for dialog.
const int kDialogDefaultWidth = 450;

}

UrlPickerDialogGtk::UrlPickerDialogGtk(UrlPickerCallback* callback,
                                       Profile* profile,
                                       GtkWindow* parent)
    : profile_(profile),
      callback_(callback) {
  dialog_ = gtk_dialog_new_with_buttons(
      l10n_util::GetStringUTF8(IDS_ASI_ADD_TITLE).c_str(),
      parent,
      // Non-modal.
      GTK_DIALOG_NO_SEPARATOR,
      GTK_STOCK_CANCEL,
      GTK_RESPONSE_CANCEL,
      NULL);

  add_button_ = gtk_dialog_add_button(GTK_DIALOG(dialog_),
                                      GTK_STOCK_ADD, GTK_RESPONSE_OK);
  gtk_dialog_set_default_response(GTK_DIALOG(dialog_), GTK_RESPONSE_OK);


  GtkWidget* url_hbox = gtk_hbox_new(FALSE, kLabelSpacing);
  GtkWidget* url_label = gtk_label_new(
      l10n_util::GetStringUTF8(IDS_ASI_URL).c_str());
  gtk_box_pack_start(GTK_BOX(url_hbox), url_label,
                     FALSE, FALSE, 0);
  url_entry_ = gtk_entry_new();
  gtk_entry_set_activates_default(GTK_ENTRY(url_entry_), TRUE);
  g_signal_connect(G_OBJECT(url_entry_), "changed",
                   G_CALLBACK(OnUrlEntryChanged), this);
  gtk_box_pack_start(GTK_BOX(url_hbox), url_entry_,
                     TRUE, TRUE, 0);
  gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog_)->vbox), url_hbox);

  gtk_window_set_default_size(GTK_WINDOW(dialog_), kDialogDefaultWidth, -1);
  gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(dialog_)->vbox), kDialogSpacing);

  EnableControls();

  gtk_widget_show_all(dialog_);

  g_signal_connect(dialog_, "response", G_CALLBACK(OnResponse), this);
  g_signal_connect(dialog_, "destroy", G_CALLBACK(OnWindowDestroy), this);
}

UrlPickerDialogGtk::~UrlPickerDialogGtk() {
  delete callback_;
}

void UrlPickerDialogGtk::AddURL() {
  GURL url(URLFixerUpper::FixupURL(
      gtk_entry_get_text(GTK_ENTRY(url_entry_)), ""));
  callback_->Run(url);
}

void UrlPickerDialogGtk::EnableControls() {
  const gchar* text = gtk_entry_get_text(GTK_ENTRY(url_entry_));
  gtk_widget_set_sensitive(add_button_, text && *text);
}

// static
void UrlPickerDialogGtk::OnUrlEntryChanged(GtkEditable* editable,
                                           UrlPickerDialogGtk* window) {
  window->EnableControls();
}

// static
void UrlPickerDialogGtk::OnResponse(GtkDialog* dialog, int response_id,
                                    UrlPickerDialogGtk* window) {
  if (response_id == GTK_RESPONSE_OK) {
    window->AddURL();
  }
  gtk_widget_destroy(window->dialog_);
}

// static
void UrlPickerDialogGtk::OnWindowDestroy(GtkWidget* widget,
                                         UrlPickerDialogGtk* window) {
  MessageLoop::current()->DeleteSoon(FROM_HERE, window);
}
