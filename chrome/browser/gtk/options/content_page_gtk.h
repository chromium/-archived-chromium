// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_OPTIONS_CONTENT_PAGE_GTK_H_
#define CHROME_BROWSER_GTK_OPTIONS_CONTENT_PAGE_GTK_H_

#include <gtk/gtk.h>

#include "chrome/browser/options_page_base.h"
#include "chrome/browser/profile.h"
#include "chrome/common/pref_member.h"

class ContentPageGtk : public OptionsPageBase {
 public:
  explicit ContentPageGtk(Profile* profile);
  ~ContentPageGtk();

  GtkWidget* get_page_widget() const {
    return page_;
  }

 private:
  // Overridden from OptionsPageBase.
  virtual void NotifyPrefChanged(const std::wstring* pref_name);

  // Initialize the option group widgets, return their container.
  GtkWidget* InitPasswordSavingGroup();
  GtkWidget* InitFormAutofillGroup();
  GtkWidget* InitBrowsingDataGroup();
  GtkWidget* InitThemesGroup();

  // Callback for import button.
  static void OnImportButtonClicked(GtkButton* widget, ContentPageGtk* page);

  // Callback for clear data button.
  static void OnClearBrowsingDataButtonClicked(GtkButton* widget,
                                               ContentPageGtk* page);

  // Callback for the GTK theme button.
  static void OnGtkThemeButtonClicked(GtkButton* widget,
                                      ContentPageGtk* page);

  // Callback for reset default theme button.
  static void OnResetDefaultThemeButtonClicked(GtkButton* widget,
                                               ContentPageGtk* page);

  // Callback for passwords exceptions button.
  static void OnPasswordsExceptionsButtonClicked(GtkButton* widget,
                                                 ContentPageGtk* page);

  // Callback for password radio buttons.
  static void OnPasswordRadioToggled(GtkToggleButton* widget,
                                     ContentPageGtk* page);

  // Callback for form autofill radio buttons.
  static void OnAutofillRadioToggled(GtkToggleButton* widget,
                                     ContentPageGtk* page);

  // Widgets for the Password saving group.
  GtkWidget* passwords_asktosave_radio_;
  GtkWidget* passwords_neversave_radio_;

  // Widget for the Form Autofill group.
  GtkWidget* form_autofill_asktosave_radio_;
  GtkWidget* form_autofill_neversave_radio_;

  // The parent GtkTable widget
  GtkWidget* page_;

  // Pref members.
  BooleanPrefMember ask_to_save_passwords_;
  BooleanPrefMember ask_to_save_form_autofill_;

  // Flag to ignore gtk callbacks while we are loading prefs, to avoid
  // then turning around and saving them again.
  bool initializing_;

  DISALLOW_COPY_AND_ASSIGN(ContentPageGtk);
};

#endif  // CHROME_BROWSER_GTK_OPTIONS_CONTENT_PAGE_GTK_H_
