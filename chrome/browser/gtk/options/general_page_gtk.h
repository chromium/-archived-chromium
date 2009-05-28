// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_OPTIONS_GENERAL_PAGE_GTK_H_
#define CHROME_BROWSER_GTK_OPTIONS_GENERAL_PAGE_GTK_H_

#include <gtk/gtk.h>

#include "chrome/browser/options_page_base.h"
#include "chrome/browser/profile.h"
#include "chrome/common/pref_member.h"
#include "googleurl/src/gurl.h"

class GeneralPageGtk : public OptionsPageBase {
 public:
  explicit GeneralPageGtk(Profile* profile);
  ~GeneralPageGtk();

  GtkWidget* get_page_widget() const {
    return page_;
  }

 private:
  // Overridden from OptionsPageBase
  virtual void NotifyPrefChanged(const std::wstring* pref_name);
  virtual void HighlightGroup(OptionsGroup highlight_group);

  // Initialize the option group widgets, return their container
  GtkWidget* InitStartupGroup();
  GtkWidget* InitHomepageGroup();
  GtkWidget* InitDefaultSearchGroup();
  GtkWidget* InitDefaultBrowserGroup();

  // Saves the startup preference from the values in the ui
  void SaveStartupPref();

  // Sets the home page preferences for kNewTabPageIsHomePage and kHomePage.
  // If a blank string is passed in we revert to using NewTab page as the Home
  // page. When setting the Home Page to NewTab page, we preserve the old value
  // of kHomePage (we don't overwrite it).
  void SetHomepage(const GURL& homepage);

  // Sets the home page pref using the value in the entry box
  void SetHomepageFromEntry();

  // Callback for startup radio buttons
  static void OnStartupRadioToggled(GtkToggleButton* toggle_button,
                                    GeneralPageGtk* general_page);

  // Callback for new tab behavior radio buttons
  static void OnNewTabIsHomePageToggled(GtkToggleButton* toggle_button,
                                        GeneralPageGtk* general_page);

  // Callback for homepage URL entry
  static void OnHomepageUseUrlEntryChanged(GtkEditable* editable,
                                           GeneralPageGtk* general_page);

  // Callback for Show Home Button option
  static void OnShowHomeButtonToggled(GtkToggleButton* toggle_button,
                                      GeneralPageGtk* general_page);

  // Callback for use as default browser button
  static void OnBrowserUseAsDefaultClicked(GtkButton* button,
                                           GeneralPageGtk* general_page);

  // Enables/Disables the controls associated with the custom start pages
  // option if that preference is not selected.
  void EnableCustomHomepagesControls(bool enable);

  // Sets the UI state to match
  void SetDefaultBrowserUIState(bool is_default);

  // Widgets of the startup group
  GtkWidget* startup_homepage_radio_;
  GtkWidget* startup_last_session_radio_;
  GtkWidget* startup_custom_radio_;
  GtkWidget* startup_custom_pages_tree_;
  GtkWidget* startup_add_custom_page_button_;
  GtkWidget* startup_remove_custom_page_button_;
  GtkWidget* startup_use_current_page_button_;

  // Widgets and prefs of the homepage group
  GtkWidget* homepage_use_newtab_radio_;
  GtkWidget* homepage_use_url_radio_;
  GtkWidget* homepage_use_url_entry_;
  GtkWidget* homepage_show_home_button_checkbox_;
  BooleanPrefMember new_tab_page_is_home_page_;
  StringPrefMember homepage_;
  BooleanPrefMember show_home_button_;

  // Widgets of the default search group
  GtkWidget* default_search_engine_combobox_;
  GtkWidget* default_search_manage_engines_button_;

  // Widgets of the default browser group
  GtkWidget* default_browser_status_label_;
  GtkWidget* default_browser_use_as_default_button_;

  // The parent GtkTable widget
  GtkWidget* page_;

  DISALLOW_COPY_AND_ASSIGN(GeneralPageGtk);
};

#endif  // CHROME_BROWSER_GTK_OPTIONS_GENERAL_PAGE_GTK_H_
