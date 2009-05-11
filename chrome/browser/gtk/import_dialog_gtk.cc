// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/import_dialog_gtk.h"

#include <gtk/gtk.h>

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "chrome/browser/profile.h"
#include "grit/generated_resources.h"

// static
void ImportDialogGtk::Show(GtkWindow* parent, Profile* profile) {
  new ImportDialogGtk(parent, profile);
}

ImportDialogGtk::ImportDialogGtk(GtkWindow* parent, Profile* profile) :
    profile_(profile), importer_host_(new ImporterHost()) {
  // Build the dialog.
  GtkWidget* dialog = gtk_dialog_new_with_buttons(
      l10n_util::GetStringUTF8(IDS_IMPORT_SETTINGS_TITLE).c_str(),
      parent,
      (GtkDialogFlags) (GTK_DIALOG_MODAL | GTK_DIALOG_NO_SEPARATOR),
      l10n_util::GetStringUTF8(IDS_IMPORT_COMMIT).c_str(),
      GTK_RESPONSE_ACCEPT,
      l10n_util::GetStringUTF8(IDS_CANCEL).c_str(),
      GTK_RESPONSE_REJECT,
      NULL);

  //TODO(rahulk): find how to set size properly so that the dialog box width is
  // atleast enough to display full title.
  gtk_widget_set_size_request(dialog, 300, -1);

  GtkWidget* content_area = GTK_DIALOG(dialog)->vbox;
  GtkWidget* alignment = gtk_alignment_new(0.0, 0.0, 0.0, 0.0);
  gtk_box_pack_start(GTK_BOX(content_area), alignment, TRUE, TRUE, 0);

  GtkWidget* vbox = gtk_vbox_new(FALSE, 0);
  gtk_container_add(GTK_CONTAINER(alignment), vbox);

  GtkWidget* combo_hbox = gtk_hbox_new(FALSE, 5);
  GtkWidget* from = gtk_label_new(
      l10n_util::GetStringUTF8(IDS_IMPORT_FROM_LABEL).c_str());
  gtk_box_pack_start(GTK_BOX(combo_hbox), from, TRUE, TRUE, 5);

  combo_ = gtk_combo_box_new_text();
  int profiles_count = importer_host_->GetAvailableProfileCount();
  for (int i = 0; i < profiles_count; i++) {
    std::wstring profile = importer_host_->GetSourceProfileNameAt(i);
    gtk_combo_box_append_text(GTK_COMBO_BOX(combo_),
                              WideToUTF8(profile).c_str());
  }
  gtk_combo_box_set_active(GTK_COMBO_BOX(combo_), 0);
  gtk_box_pack_start(GTK_BOX(combo_hbox), combo_, TRUE, TRUE, 5);
  gtk_box_pack_start(GTK_BOX(vbox), combo_hbox, TRUE, TRUE, 5);

  GtkWidget* description = gtk_label_new(
      l10n_util::GetStringUTF8(IDS_IMPORT_ITEMS_LABEL).c_str());
  gtk_box_pack_start(GTK_BOX(vbox), description, TRUE, TRUE, 5);

  GtkWidget* text_alignment = gtk_alignment_new(0.0, 0.0, 1.0, 1.0);
  gtk_alignment_set_padding(GTK_ALIGNMENT(text_alignment), 0, 0, 25, 0);
  GtkWidget* text_vbox = gtk_vbox_new(FALSE, 0);
  gtk_container_add(GTK_CONTAINER(text_alignment), text_vbox);
  gtk_box_pack_start(GTK_BOX(vbox), text_alignment, TRUE, TRUE, 0);

  bookmarks_ = gtk_check_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_IMPORT_FAVORITES_CHKBOX).c_str());
  gtk_box_pack_start(GTK_BOX(text_vbox), bookmarks_, TRUE, TRUE, 5);

  search_engines_ = gtk_check_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_IMPORT_SEARCH_ENGINES_CHKBOX).c_str());
  gtk_box_pack_start(GTK_BOX(text_vbox), search_engines_, TRUE, TRUE, 5);

  passwords_ = gtk_check_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_IMPORT_PASSWORDS_CHKBOX).c_str());
  gtk_box_pack_start(GTK_BOX(text_vbox), passwords_, TRUE, TRUE, 5);

  history_ = gtk_check_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_IMPORT_HISTORY_CHKBOX).c_str());
  gtk_box_pack_start(GTK_BOX(text_vbox), history_, TRUE, TRUE, 5);

  g_signal_connect(dialog, "response",
                   G_CALLBACK(HandleOnResponseDialog), this);
  gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);
  gtk_widget_show_all(dialog);
}

void ImportDialogGtk::OnDialogResponse(GtkWidget* widget, int response) {
  if (response == GTK_RESPONSE_ACCEPT) {
    uint16 items = NONE;
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(bookmarks_)))
      items |= FAVORITES;
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(search_engines_)))
      items |= SEARCH_ENGINES;
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(passwords_)))
      items |= PASSWORDS;
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(history_)))
      items |= HISTORY;

    const ProfileInfo& source_profile = importer_host_->GetSourceProfileInfoAt(
        gtk_combo_box_get_active(GTK_COMBO_BOX(combo_)));

    // TODO(rahulk): We should not do the import on this thread. Instead
    // we need to start this asynchronously and launch a UI that shows the
    // progress of import.
    importer_host_->StartImportSettings(source_profile, items,
                                        new ProfileWriter(profile_), false);
  }

  delete this;
  gtk_widget_destroy(GTK_WIDGET(widget));
}
