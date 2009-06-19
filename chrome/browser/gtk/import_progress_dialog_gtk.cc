// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/import_progress_dialog_gtk.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "chrome/common/gtk_util.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"

namespace {
void SetItemImportStatus(GtkWidget* chkbox, int res_id, bool is_done) {
  std::string status = l10n_util::GetStringUTF8(res_id);
  // Windows version of this has fancy throbbers to indicate progress. Here
  // we rely on text until we can have something equivalent on Linux.
  if (is_done)
    status.append(" done.");
  else
    status.append(" importing...");
  gtk_button_set_label(GTK_BUTTON(chkbox), status.c_str());
}
}  // namespace

// static
void ImportProgressDialogGtk::StartImport(GtkWindow* parent,
                                          int16 items,
                                          ImporterHost* importer_host,
                                          const ProfileInfo& browser_profile,
                                          Profile* profile,
                                          ImportObserver* observer,
                                          bool first_run) {
  ImportProgressDialogGtk* v = new ImportProgressDialogGtk(
      WideToUTF16(browser_profile.description), items, importer_host, observer,
      parent, browser_profile.browser_type == BOOKMARKS_HTML);

  // In headless mode it means that we don't show the progress window, but it
  // still need it to exist. No user interaction will be required.
  if (!importer_host->is_headless())
    v->ShowDialog();

  importer_host->StartImportSettings(browser_profile, profile, items,
                                     new ProfileWriter(profile),
                                     first_run);
}

////////////////////////////////////////////////////////////////////////////////
// ImporterHost::Observer implementation:
void ImportProgressDialogGtk::ImportItemStarted(ImportItem item) {
  DCHECK(items_ & item);
  switch (item) {
    case FAVORITES:
      SetItemImportStatus(bookmarks_,
                          IDS_IMPORT_PROGRESS_STATUS_BOOKMARKS, false);
      break;
    case SEARCH_ENGINES:
      SetItemImportStatus(search_engines_,
                          IDS_IMPORT_PROGRESS_STATUS_SEARCH, false);
      break;
    case PASSWORDS:
      SetItemImportStatus(passwords_,
                          IDS_IMPORT_PROGRESS_STATUS_PASSWORDS, false);
      break;
    case HISTORY:
      SetItemImportStatus(history_,
                          IDS_IMPORT_PROGRESS_STATUS_HISTORY, false);
      break;
    default:
      break;
  }
}

void ImportProgressDialogGtk::ImportItemEnded(ImportItem item) {
  DCHECK(items_ & item);
  switch (item) {
    case FAVORITES:
      SetItemImportStatus(bookmarks_,
                          IDS_IMPORT_PROGRESS_STATUS_BOOKMARKS, true);
      break;
    case SEARCH_ENGINES:
      SetItemImportStatus(search_engines_,
                          IDS_IMPORT_PROGRESS_STATUS_SEARCH, true);
      break;
    case PASSWORDS:
      SetItemImportStatus(passwords_,
                          IDS_IMPORT_PROGRESS_STATUS_PASSWORDS, true);
      break;
    case HISTORY:
      SetItemImportStatus(history_,
                          IDS_IMPORT_PROGRESS_STATUS_HISTORY, true);
      break;
    default:
      break;
  }
}

void ImportProgressDialogGtk::ImportStarted() {
  importing_ = true;
}

void ImportProgressDialogGtk::ImportEnded() {
  importing_ = false;
  importer_host_->SetObserver(NULL);
  if (observer_)
    observer_->ImportComplete();
  CloseDialog();
}

ImportProgressDialogGtk::ImportProgressDialogGtk(const string16& source_profile,
    int16 items, ImporterHost* importer_host, ImportObserver* observer,
    GtkWindow* parent, bool bookmarks_import) :
    parent_(parent), importing_(true), observer_(observer),
    items_(items), importer_host_(importer_host) {
  importer_host_->SetObserver(this);

  // Build the dialog.
  dialog_ = gtk_dialog_new_with_buttons(
      l10n_util::GetStringUTF8(IDS_IMPORT_PROGRESS_TITLE).c_str(),
      parent_,
      (GtkDialogFlags) (GTK_DIALOG_MODAL | GTK_DIALOG_NO_SEPARATOR),
      GTK_STOCK_CANCEL,
      GTK_RESPONSE_REJECT,
      NULL);
  importer_host_->set_parent_window(GTK_WINDOW(dialog_));

  GtkWidget* content_area = GTK_DIALOG(dialog_)->vbox;
  gtk_box_set_spacing(GTK_BOX(content_area), gtk_util::kContentAreaSpacing);

  GtkWidget* import_info = gtk_label_new(
      l10n_util::GetStringFUTF8(IDS_IMPORT_PROGRESS_INFO,
                                source_profile).c_str());
  gtk_label_set_single_line_mode(GTK_LABEL(import_info), FALSE);
  gtk_box_pack_start(GTK_BOX(content_area), import_info, FALSE, FALSE, 0);

  bookmarks_ = gtk_check_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_IMPORT_PROGRESS_STATUS_BOOKMARKS).c_str());
  gtk_box_pack_start(GTK_BOX(content_area), bookmarks_, FALSE, FALSE, 0);
  gtk_widget_set_sensitive(bookmarks_, FALSE);
  if (items_ & FAVORITES)
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(bookmarks_), TRUE);

  search_engines_ = gtk_check_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_IMPORT_PROGRESS_STATUS_SEARCH).c_str());
  gtk_box_pack_start(GTK_BOX(content_area), search_engines_, FALSE, FALSE, 0);
  gtk_widget_set_sensitive(search_engines_, FALSE);
  if (items_ & SEARCH_ENGINES)
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(search_engines_), TRUE);

  passwords_ = gtk_check_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_IMPORT_PROGRESS_STATUS_PASSWORDS).c_str());
  gtk_box_pack_start(GTK_BOX(content_area), passwords_, FALSE, FALSE, 0);
  gtk_widget_set_sensitive(passwords_, FALSE);
  if (items_ & PASSWORDS)
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(passwords_), TRUE);

  history_ = gtk_check_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_IMPORT_PROGRESS_STATUS_HISTORY).c_str());
  gtk_box_pack_start(GTK_BOX(content_area), history_, FALSE, FALSE, 0);
  gtk_widget_set_sensitive(history_, FALSE);
  if (items_ & HISTORY)
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(history_), TRUE);

  g_signal_connect(dialog_, "response",
                   G_CALLBACK(HandleOnResponseDialog), this);
  gtk_window_set_resizable(GTK_WINDOW(dialog_), FALSE);
}

void ImportProgressDialogGtk::CloseDialog() {
  gtk_widget_destroy(dialog_);
  dialog_ = NULL;
  delete this;
}

void ImportProgressDialogGtk::OnDialogResponse(GtkWidget* widget,
                                               int response) {
  if (!importing_) {
    CloseDialog();
    return;
  }

  // Only response to the dialog is to close it so we hide immediately.
  gtk_widget_hide_all(dialog_);
  if (response == GTK_RESPONSE_REJECT)
    importer_host_->Cancel();
}

void ImportProgressDialogGtk::ShowDialog() {
  gtk_widget_show_all(dialog_);
}


void StartImportingWithUI(GtkWindow* parent,
                          int16 items,
                          ImporterHost* importer_host,
                          const ProfileInfo& browser_profile,
                          Profile* profile,
                          ImportObserver* observer,
                          bool first_run) {
  DCHECK(items != 0);
  ImportProgressDialogGtk::StartImport(parent, items, importer_host,
                                       browser_profile, profile, observer,
                                       first_run);
}
