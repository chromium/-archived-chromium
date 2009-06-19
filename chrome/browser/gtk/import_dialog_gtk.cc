// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/import_dialog_gtk.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "chrome/common/gtk_util.h"
#include "grit/generated_resources.h"

// static
void ImportDialogGtk::Show(GtkWindow* parent, Profile* profile) {
  new ImportDialogGtk(parent, profile);
}

////////////////////////////////////////////////////////////////////////////////
// ImportObserver implementation:
void ImportDialogGtk::ImportCanceled() {
  ImportComplete();
}

void ImportDialogGtk::ImportComplete() {
  gtk_widget_destroy(dialog_);
  delete this;
}

ImportDialogGtk::ImportDialogGtk(GtkWindow* parent, Profile* profile) :
    parent_(parent), profile_(profile), importer_host_(new ImporterHost()) {
  // Build the dialog.
  dialog_ = gtk_dialog_new_with_buttons(
      l10n_util::GetStringUTF8(IDS_IMPORT_SETTINGS_TITLE).c_str(),
      parent,
      (GtkDialogFlags) (GTK_DIALOG_MODAL | GTK_DIALOG_NO_SEPARATOR),
      GTK_STOCK_CANCEL,
      GTK_RESPONSE_REJECT,
      NULL);
  importer_host_->set_parent_window(GTK_WINDOW(dialog_));

  // Add import button separately as we might need to disable it, if
  // no supported browsers found.
  GtkWidget* import_button = gtk_dialog_add_button(GTK_DIALOG(dialog_),
      l10n_util::GetStringUTF8(IDS_IMPORT_COMMIT).c_str(),
      GTK_RESPONSE_ACCEPT);

  // TODO(rahulk): find how to set size properly so that the dialog
  // box width is at least enough to display full title.
  gtk_widget_set_size_request(dialog_, 300, -1);

  GtkWidget* content_area = GTK_DIALOG(dialog_)->vbox;
  gtk_box_set_spacing(GTK_BOX(content_area), gtk_util::kContentAreaSpacing);

  GtkWidget* combo_hbox = gtk_hbox_new(FALSE, gtk_util::kLabelSpacing);
  GtkWidget* from = gtk_label_new(
      l10n_util::GetStringUTF8(IDS_IMPORT_FROM_LABEL).c_str());
  gtk_box_pack_start(GTK_BOX(combo_hbox), from, FALSE, FALSE, 0);

  combo_ = gtk_combo_box_new_text();
  gtk_box_pack_start(GTK_BOX(combo_hbox), combo_, TRUE, TRUE, 0);

  gtk_box_pack_start(GTK_BOX(content_area), combo_hbox, FALSE, FALSE, 0);

  GtkWidget* vbox = gtk_vbox_new(FALSE, gtk_util::kControlSpacing);

  GtkWidget* description = gtk_label_new(
      l10n_util::GetStringUTF8(IDS_IMPORT_ITEMS_LABEL).c_str());
  gtk_misc_set_alignment(GTK_MISC(description), 0, 0);
  gtk_box_pack_start(GTK_BOX(vbox), description, FALSE, FALSE, 0);

  bookmarks_ = gtk_check_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_IMPORT_FAVORITES_CHKBOX).c_str());
  gtk_box_pack_start(GTK_BOX(vbox), bookmarks_, FALSE, FALSE, 0);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(bookmarks_), TRUE);

  search_engines_ = gtk_check_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_IMPORT_SEARCH_ENGINES_CHKBOX).c_str());
  gtk_box_pack_start(GTK_BOX(vbox), search_engines_, FALSE, FALSE, 0);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(search_engines_), TRUE);

  passwords_ = gtk_check_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_IMPORT_PASSWORDS_CHKBOX).c_str());
  gtk_box_pack_start(GTK_BOX(vbox), passwords_, FALSE, FALSE, 0);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(passwords_), TRUE);

  history_ = gtk_check_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_IMPORT_HISTORY_CHKBOX).c_str());
  gtk_box_pack_start(GTK_BOX(vbox), history_, FALSE, FALSE, 0);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(history_), TRUE);

  gtk_box_pack_start(GTK_BOX(content_area), vbox, FALSE, FALSE, 0);

  // Detect any supported browsers that we can import from and fill
  // up the combo box. If none found, disable all controls except cancel.
  int profiles_count = importer_host_->GetAvailableProfileCount();
  if (profiles_count > 0) {
    for (int i = 0; i < profiles_count; i++) {
      std::wstring profile = importer_host_->GetSourceProfileNameAt(i);
      gtk_combo_box_append_text(GTK_COMBO_BOX(combo_),
                                WideToUTF8(profile).c_str());
    }
  } else {
    gtk_combo_box_append_text(GTK_COMBO_BOX(combo_),
        l10n_util::GetStringUTF8(IDS_IMPORT_NO_PROFILE_FOUND).c_str());
    gtk_widget_set_sensitive(bookmarks_, FALSE);
    gtk_widget_set_sensitive(search_engines_, FALSE);
    gtk_widget_set_sensitive(passwords_, FALSE);
    gtk_widget_set_sensitive(history_, FALSE);
    gtk_widget_set_sensitive(import_button, FALSE);
  }
  gtk_combo_box_set_active(GTK_COMBO_BOX(combo_), 0);

  g_signal_connect(dialog_, "response",
                   G_CALLBACK(HandleOnResponseDialog), this);
  gtk_window_set_resizable(GTK_WINDOW(dialog_), FALSE);
  gtk_widget_show_all(dialog_);
}

void ImportDialogGtk::OnDialogResponse(GtkWidget* widget, int response) {
  gtk_widget_hide_all(dialog_);
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

    if (items == 0) {
      ImportComplete();
    } else {
      const ProfileInfo& source_profile =
          importer_host_->GetSourceProfileInfoAt(
          gtk_combo_box_get_active(GTK_COMBO_BOX(combo_)));
      StartImportingWithUI(parent_, items, importer_host_.get(),
                           source_profile, profile_, this, false);
    }
  } else {
    ImportCanceled();
  }
}
