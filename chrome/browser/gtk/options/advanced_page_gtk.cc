// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/options/advanced_page_gtk.h"

#include "app/l10n_util.h"
#include "chrome/browser/options_util.h"
#include "chrome/common/gtk_util.h"
#include "grit/generated_resources.h"
#include "grit/chromium_strings.h"

AdvancedPageGtk::AdvancedPageGtk(Profile* profile)
    : OptionsPageBase(profile),
      advanced_contents_(profile) {
  Init();
}

AdvancedPageGtk::~AdvancedPageGtk() {
}

void AdvancedPageGtk::Init() {
  page_ = gtk_vbox_new(FALSE, gtk_util::kControlSpacing);
  gtk_container_set_border_width(GTK_CONTAINER(page_),
                                 gtk_util::kContentAreaBorder);

  GtkWidget* scroll_window = gtk_scrolled_window_new(NULL, NULL);
  gtk_container_add(GTK_CONTAINER(page_), scroll_window);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll_window),
                                 GTK_POLICY_NEVER,
                                 GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scroll_window),
                                      GTK_SHADOW_ETCHED_IN);
  gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scroll_window),
                                        advanced_contents_.get_page_widget());

  GtkWidget* button_box = gtk_hbutton_box_new();
  gtk_button_box_set_layout(GTK_BUTTON_BOX(button_box), GTK_BUTTONBOX_END);
  GtkWidget* reset_button = gtk_button_new_with_label(
        l10n_util::GetStringUTF8(IDS_OPTIONS_RESET).c_str());
  g_signal_connect(G_OBJECT(reset_button), "clicked",
                   G_CALLBACK(OnResetToDefaultsClicked), this);
  gtk_container_add(GTK_CONTAINER(button_box), reset_button);
  gtk_box_pack_start(GTK_BOX(page_), button_box, FALSE, FALSE, 0);
}

// static
void AdvancedPageGtk::OnResetToDefaultsClicked(
    GtkButton* button, AdvancedPageGtk* advanced_page) {
  advanced_page->UserMetricsRecordAction(L"Options_ResetToDefaults", NULL);
  GtkWidget* dialog_ = gtk_message_dialog_new(
      GTK_WINDOW(gtk_widget_get_toplevel(advanced_page->page_)),
      static_cast<GtkDialogFlags>(GTK_DIALOG_MODAL),
      GTK_MESSAGE_QUESTION,
      GTK_BUTTONS_NONE,
      "%s",
      l10n_util::GetStringUTF8(IDS_OPTIONS_RESET_MESSAGE).c_str());
  gtk_dialog_add_buttons(
      GTK_DIALOG(dialog_),
      l10n_util::GetStringUTF8(IDS_OPTIONS_RESET_CANCELLABEL).c_str(),
      GTK_RESPONSE_CANCEL,
      l10n_util::GetStringUTF8(IDS_OPTIONS_RESET_OKLABEL).c_str(),
      GTK_RESPONSE_OK,
      NULL);
  gtk_window_set_title(GTK_WINDOW(dialog_),
      l10n_util::GetStringUTF8(IDS_PRODUCT_NAME).c_str());
  g_signal_connect(dialog_, "response",
                   G_CALLBACK(OnResetToDefaultsResponse), advanced_page);

  gtk_widget_show_all(dialog_);
}

// static
void AdvancedPageGtk::OnResetToDefaultsResponse(
    GtkDialog* dialog, int response_id, AdvancedPageGtk* advanced_page) {
  if (response_id == GTK_RESPONSE_OK) {
    OptionsUtil::ResetToDefaults(advanced_page->profile());
  }
  gtk_widget_destroy(GTK_WIDGET(dialog));
}
