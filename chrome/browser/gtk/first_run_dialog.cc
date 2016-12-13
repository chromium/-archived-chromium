// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/first_run_dialog.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/message_loop.h"
#include "chrome/app/breakpad_linux.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/common/gtk_util.h"
#include "chrome/installer/util/google_update_settings.h"
#include "grit/generated_resources.h"
#include "grit/google_chrome_strings.h"

// static
bool FirstRunDialog::Show(Profile* profile) {
  int response = -1;
  new FirstRunDialog(profile, response);
  return (response == GTK_RESPONSE_ACCEPT);
}

FirstRunDialog::FirstRunDialog(Profile* profile, int& response)
    : dialog_(NULL), report_crashes_(NULL), make_default_(NULL),
      import_data_(NULL), import_profile_(NULL), profile_(profile),
      response_(response), importer_host_(new ImporterHost()) {
  dialog_ = gtk_dialog_new_with_buttons(
      l10n_util::GetStringUTF8(IDS_FIRSTRUN_DLG_TITLE).c_str(),
      NULL,  // No parent
      (GtkDialogFlags) (GTK_DIALOG_MODAL | GTK_DIALOG_NO_SEPARATOR),
      l10n_util::GetStringUTF8(IDS_FIRSTRUN_DLG_OK).c_str(),
      GTK_RESPONSE_ACCEPT,
      l10n_util::GetStringUTF8(IDS_FIRSTRUN_DLG_CANCEL).c_str(),
      GTK_RESPONSE_REJECT,
      NULL);
  gtk_window_set_resizable(GTK_WINDOW(dialog_), FALSE);
  g_signal_connect(G_OBJECT(dialog_), "delete-event",
                   G_CALLBACK(gtk_widget_hide_on_delete), NULL);

  GtkWidget* content_area = GTK_DIALOG(dialog_)->vbox;
  gtk_box_set_spacing(GTK_BOX(content_area), 18);

  GtkWidget* vbox = gtk_vbox_new(FALSE, 12);
  // Force a size on the vbox so the labels wrap.
  gtk_widget_set_size_request(vbox, 350, -1);

#if defined(GOOGLE_CHROME_BUILD)
  // TODO(port): remove this warning before beta release when we have all the
  // privacy features working.
  GtkWidget* privacy_label = gtk_label_new(
      "This version of Google Chrome for Linux is not appropriate for "
      "general consumer use.  Certain privacy features are unavailable "
      "at this time as described in our privacy policy at");
  gtk_misc_set_alignment(GTK_MISC(privacy_label), 0, 0);
  gtk_label_set_line_wrap(GTK_LABEL(privacy_label), TRUE);
  gtk_box_pack_start(GTK_BOX(vbox), privacy_label, FALSE, FALSE, 0);

  GtkWidget* url_label = gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(url_label),
      "<tt>http://www.google.com/chrome/intl/en/privacy_linux.html</tt>");
  // Set selectable to allow copy and paste.
  gtk_label_set_selectable(GTK_LABEL(url_label), TRUE);
  gtk_box_pack_start(GTK_BOX(vbox), url_label, FALSE, FALSE, 0);

  report_crashes_ = gtk_check_button_new();
  GtkWidget* check_label = gtk_label_new(
      l10n_util::GetStringUTF8(IDS_OPTIONS_ENABLE_LOGGING).c_str());
  gtk_label_set_line_wrap(GTK_LABEL(check_label), TRUE);
  gtk_container_add(GTK_CONTAINER(report_crashes_), check_label);
  gtk_box_pack_start(GTK_BOX(vbox), report_crashes_, FALSE, FALSE, 0);
#endif

  make_default_ = gtk_check_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_FR_CUSTOMIZE_DEFAULT_BROWSER).c_str());
  gtk_box_pack_start(GTK_BOX(vbox), make_default_, FALSE, FALSE, 0);

  GtkWidget* combo_hbox = gtk_hbox_new(FALSE, gtk_util::kLabelSpacing);
  import_data_ = gtk_check_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_FR_CUSTOMIZE_IMPORT).c_str());
  gtk_box_pack_start(GTK_BOX(combo_hbox), import_data_, FALSE, FALSE, 0);
  import_profile_ = gtk_combo_box_new_text();
  gtk_box_pack_start(GTK_BOX(combo_hbox), import_profile_, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), combo_hbox, FALSE, FALSE, 0);

  // Detect any supported browsers that we can import from and fill
  // up the combo box. If none found, disable import data checkbox.
  int profiles_count = importer_host_->GetAvailableProfileCount();
  if (profiles_count > 0) {
    for (int i = 0; i < profiles_count; i++) {
      std::wstring profile = importer_host_->GetSourceProfileNameAt(i);
      gtk_combo_box_append_text(GTK_COMBO_BOX(import_profile_),
                                WideToUTF8(profile).c_str());
    }
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(import_data_), TRUE);
    gtk_combo_box_set_active(GTK_COMBO_BOX(import_profile_), 0);
  } else {
    gtk_combo_box_append_text(GTK_COMBO_BOX(import_profile_),
        l10n_util::GetStringUTF8(IDS_IMPORT_NO_PROFILE_FOUND).c_str());
    gtk_combo_box_set_active(GTK_COMBO_BOX(import_profile_), 0);
    gtk_widget_set_sensitive(import_data_, FALSE);
    gtk_widget_set_sensitive(import_profile_, FALSE);
  }

  gtk_box_pack_start(GTK_BOX(content_area), vbox, FALSE, FALSE, 0);

  // TODO(port): it should be sufficient to just run the dialog:
  // int response = gtk_dialog_run(GTK_DIALOG(dialog));
  // but that spins a nested message loop and hoses us.  :(
  // http://code.google.com/p/chromium/issues/detail?id=12552
  // Instead, run a loop and extract the response manually.
  g_signal_connect(G_OBJECT(dialog_), "response",
                   G_CALLBACK(HandleOnResponseDialog), this);
  gtk_widget_show_all(dialog_);
  MessageLoop::current()->Run();
}

void FirstRunDialog::OnDialogResponse(GtkWidget* widget, int response) {
  gtk_widget_hide_all(dialog_);
  response_ = response;

  if (response == GTK_RESPONSE_ACCEPT) {
    // Mark that first run has ran.
    FirstRun::CreateSentinel();

    // Check if user has opted into reporting.
    if (report_crashes_ &&
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(report_crashes_))) {
      if (GoogleUpdateSettings::SetCollectStatsConsent(true))
        InitCrashReporter();
    } else {
      GoogleUpdateSettings::SetCollectStatsConsent(false);
    }

    // If selected set as default browser.
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(make_default_)))
      ShellIntegration::SetAsDefaultBrowser();

    // Import data if selected.
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(import_data_))) {
      const ProfileInfo& source_profile =
          importer_host_->GetSourceProfileInfoAt(
          gtk_combo_box_get_active(GTK_COMBO_BOX(import_profile_)));
      int items = SEARCH_ENGINES + HISTORY + FAVORITES + HOME_PAGE + PASSWORDS;
      // TODO(port): Call StartImportingWithUI here instead and launch
      // a new process that does the actual import.
      importer_host_->StartImportSettings(source_profile, profile_, items,
                                          new ProfileWriter(profile_), true);
    }
  }

  gtk_widget_destroy(dialog_);
  MessageLoop::current()->Quit();
}
