// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/fonts_languages_window.h"

#include <gtk/gtk.h>

#include "app/l10n_util.h"
#include "base/message_loop.h"
#include "chrome/browser/profile.h"
#include "chrome/common/gtk_util.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"

///////////////////////////////////////////////////////////////////////////////
// FontsLanguagesWindowGtk 
//
// The contents of the Options dialog window.

class FontsLanguagesWindowGtk {
 public:
  explicit FontsLanguagesWindowGtk(Profile* profile);
  ~FontsLanguagesWindowGtk();

  // Shows the tab corresponding to the specified |page|.
  void ShowTabPage(FontsLanguagesPage page);

 private:
  static void OnWindowDestroy(GtkWidget* widget,
                              FontsLanguagesWindowGtk* window);

  // The fonts and languages dialog.
  GtkWidget *dialog_;

  // The container of the option pages.
  GtkWidget *notebook_;

  // The Profile associated with these options.
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(FontsLanguagesWindowGtk);
};

static FontsLanguagesWindowGtk* instance_ = NULL;

///////////////////////////////////////////////////////////////////////////////
// FontsLanguagesWindowGtk, public:

FontsLanguagesWindowGtk::FontsLanguagesWindowGtk(Profile* profile)
      // Always show preferences for the original profile. Most state when off
      // the record comes from the original profile, but we explicitly use
      // the original profile to avoid potential problems.
    : profile_(profile->GetOriginalProfile()) {
  dialog_ = gtk_dialog_new_with_buttons(
      l10n_util::GetStringFUTF8(IDS_FONT_LANGUAGE_SETTING_WINDOWS_TITLE,
          l10n_util::GetStringUTF16(IDS_PRODUCT_NAME)).c_str(),
      // Prefs window is shared between all browser windows.
      NULL,
      // Non-modal.
      GTK_DIALOG_NO_SEPARATOR,
      GTK_STOCK_CLOSE,
      GTK_RESPONSE_CLOSE,
      NULL);
  gtk_window_set_default_size(GTK_WINDOW(dialog_), 500, -1);
  gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(dialog_)->vbox),
                      gtk_util::kContentAreaSpacing);

  notebook_ = gtk_notebook_new();

  // Fonts and Encoding tab.
  gtk_notebook_append_page(
      GTK_NOTEBOOK(notebook_),
      gtk_label_new("TODO content"),
      gtk_label_new(
          l10n_util::GetStringUTF8(
              IDS_FONT_LANGUAGE_SETTING_FONT_TAB_TITLE).c_str()));

  // Langauges tab.
  gtk_notebook_append_page(
      GTK_NOTEBOOK(notebook_),
      gtk_label_new("TODO content"),
      gtk_label_new(
          l10n_util::GetStringUTF8(
              IDS_FONT_LANGUAGE_SETTING_LANGUAGES_TAB_TITLE).c_str()));

  gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog_)->vbox), notebook_);

  // Show the notebook.
  gtk_widget_show_all(dialog_);

  // We only have one button and don't do any special handling, so just hook it
  // directly to gtk_widget_destroy.
  g_signal_connect_swapped(dialog_, "response", G_CALLBACK(gtk_widget_destroy),
                           dialog_);

  g_signal_connect(dialog_, "destroy", G_CALLBACK(OnWindowDestroy), this);
}

FontsLanguagesWindowGtk::~FontsLanguagesWindowGtk() {
}

void FontsLanguagesWindowGtk::ShowTabPage(FontsLanguagesPage page) {
  // Bring options window to front if it already existed and isn't already
  // in front.
  gtk_window_present(GTK_WINDOW(dialog_));

  // If the page is out of bounds, reset to the first tab.
  if (page < 0 || page >= gtk_notebook_get_n_pages(GTK_NOTEBOOK(notebook_)))
    page = FONTS_ENCODING_PAGE;

  // Switch the tab to the selected |page|.
  gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook_), page);
}

///////////////////////////////////////////////////////////////////////////////
// FontsLanguagesWindowGtk, private:

// static
void FontsLanguagesWindowGtk::OnWindowDestroy(GtkWidget* widget,
    FontsLanguagesWindowGtk* window) {
  instance_ = NULL;
  MessageLoop::current()->DeleteSoon(FROM_HERE, window);
}

void ShowFontsLanguagesWindow(gfx::NativeWindow window,
                              FontsLanguagesPage page,
                              Profile* profile) {
  DCHECK(profile);
  // If there's already an existing fonts and language window, activate it and
  // switch to the specified page.
  if (!instance_)
    instance_ = new FontsLanguagesWindowGtk(profile);

  instance_->ShowTabPage(page);
}
