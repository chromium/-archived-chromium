// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/options/general_page_gtk.h"

#include "app/l10n_util.h"
#include "chrome/browser/gtk/options/options_layout_gtk.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/browser/session_startup_pref.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/url_constants.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"

// TODO(mattm): spacing constants should be moved into a system-wide location
namespace {

// Spacing between options of the same group
const int kOptionSpacing = 6;

// Horizontal spacing between a label and it's control
const int kLabelSpacing = 12;

// Markup for the text showing the current state of the default browser
const char kDefaultBrowserLabelMarkup[] = "<span color='#%s'>%s</span>";

// Color of the default browser text when Chromium is the default browser
const char kDefaultBrowserLabelColor[] = "008700";

// Color of the default browser text when Chromium is not the default browser
const char kNotDefaultBrowserLabelColor[] = "870000";

}

///////////////////////////////////////////////////////////////////////////////
// GeneralPageGtk, public:

GeneralPageGtk::GeneralPageGtk(Profile* profile)
    : OptionsPageBase(profile)  {
  OptionsLayoutBuilderGtk options_builder(4);
  options_builder.AddOptionGroup(
      l10n_util::GetStringUTF8(IDS_OPTIONS_STARTUP_GROUP_NAME),
      InitStartupGroup());
  options_builder.AddOptionGroup(
      l10n_util::GetStringUTF8(IDS_OPTIONS_HOMEPAGE_GROUP_NAME),
      InitHomepageGroup());
  options_builder.AddOptionGroup(
      l10n_util::GetStringUTF8(IDS_OPTIONS_DEFAULTSEARCH_GROUP_NAME),
      InitDefaultSearchGroup());
  options_builder.AddOptionGroup(
      l10n_util::GetStringUTF8(IDS_OPTIONS_DEFAULTBROWSER_GROUP_NAME),
      InitDefaultBrowserGroup());
  page_ = options_builder.get_page_widget();

  profile->GetPrefs()->AddPrefObserver(prefs::kRestoreOnStartup, this);
  profile->GetPrefs()->AddPrefObserver(prefs::kURLsToRestoreOnStartup, this);

  new_tab_page_is_home_page_.Init(prefs::kHomePageIsNewTabPage,
      profile->GetPrefs(), this);
  homepage_.Init(prefs::kHomePage, profile->GetPrefs(), this);
  show_home_button_.Init(prefs::kShowHomeButton, profile->GetPrefs(), this);

  // Load initial values
  NotifyPrefChanged(NULL);
}

GeneralPageGtk::~GeneralPageGtk() {
  profile()->GetPrefs()->RemovePrefObserver(prefs::kRestoreOnStartup, this);
  profile()->GetPrefs()->RemovePrefObserver(
      prefs::kURLsToRestoreOnStartup, this);
}

///////////////////////////////////////////////////////////////////////////////
// GeneralPageGtk, OptionsPageBase overrides:

void GeneralPageGtk::NotifyPrefChanged(const std::wstring* pref_name) {
  if (!pref_name || *pref_name == prefs::kRestoreOnStartup) {
    PrefService* prefs = profile()->GetPrefs();
    const SessionStartupPref startup_pref =
        SessionStartupPref::GetStartupPref(prefs);
    switch (startup_pref.type) {
    case SessionStartupPref::DEFAULT:
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(startup_homepage_radio_),
                                   TRUE);
      EnableCustomHomepagesControls(false);
      break;

    case SessionStartupPref::LAST:
      gtk_toggle_button_set_active(
          GTK_TOGGLE_BUTTON(startup_last_session_radio_), TRUE);
      EnableCustomHomepagesControls(false);
      break;

    case SessionStartupPref::URLS:
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(startup_custom_radio_),
                                   TRUE);
      EnableCustomHomepagesControls(true);
      break;
    }
  }

  // TODO(mattm): kURLsToRestoreOnStartup

  if (!pref_name || *pref_name == prefs::kHomePageIsNewTabPage) {
    if (new_tab_page_is_home_page_.GetValue()) {
      gtk_toggle_button_set_active(
          GTK_TOGGLE_BUTTON(homepage_use_newtab_radio_), TRUE);
      gtk_widget_set_sensitive(homepage_use_url_entry_, FALSE);
    } else {
      gtk_toggle_button_set_active(
          GTK_TOGGLE_BUTTON(homepage_use_url_radio_), TRUE);
      gtk_widget_set_sensitive(homepage_use_url_entry_, TRUE);
    }
  }

  if (!pref_name || *pref_name == prefs::kHomePage) {
    bool enabled = homepage_.GetValue() !=
        UTF8ToWide(chrome::kChromeUINewTabURL);
    if (enabled)
      gtk_entry_set_text(GTK_ENTRY(homepage_use_url_entry_),
                         WideToUTF8(homepage_.GetValue()).c_str());
  }

  if (!pref_name || *pref_name == prefs::kShowHomeButton) {
    gtk_toggle_button_set_active(
        GTK_TOGGLE_BUTTON(homepage_show_home_button_checkbox_),
        show_home_button_.GetValue());
  }
}

void GeneralPageGtk::HighlightGroup(OptionsGroup highlight_group) {
  // TODO(mattm): implement group highlighting
}

///////////////////////////////////////////////////////////////////////////////
// GeneralPageGtk, private:

GtkWidget* GeneralPageGtk::InitStartupGroup() {
  GtkWidget* vbox = gtk_vbox_new(FALSE, kOptionSpacing);

  startup_homepage_radio_ = gtk_radio_button_new_with_label(NULL,
      l10n_util::GetStringUTF8(
          IDS_OPTIONS_STARTUP_SHOW_DEFAULT_AND_NEWTAB).c_str());
  g_signal_connect(G_OBJECT(startup_homepage_radio_), "toggled",
                   G_CALLBACK(OnStartupRadioToggled), this);
  gtk_container_add(GTK_CONTAINER(vbox), startup_homepage_radio_);

  startup_last_session_radio_ = gtk_radio_button_new_with_label_from_widget(
      GTK_RADIO_BUTTON(startup_homepage_radio_),
      l10n_util::GetStringUTF8(IDS_OPTIONS_STARTUP_SHOW_LAST_SESSION).c_str());
  g_signal_connect(G_OBJECT(startup_last_session_radio_), "toggled",
                   G_CALLBACK(OnStartupRadioToggled), this);
  gtk_container_add(GTK_CONTAINER(vbox), startup_last_session_radio_);

  startup_custom_radio_ = gtk_radio_button_new_with_label_from_widget(
      GTK_RADIO_BUTTON(startup_homepage_radio_),
      l10n_util::GetStringUTF8(IDS_OPTIONS_STARTUP_SHOW_PAGES).c_str());
  g_signal_connect(G_OBJECT(startup_custom_radio_), "toggled",
                   G_CALLBACK(OnStartupRadioToggled), this);
  gtk_container_add(GTK_CONTAINER(vbox), startup_custom_radio_);

  GtkWidget* url_list_container = gtk_hbox_new(FALSE, kOptionSpacing);
  gtk_container_add(GTK_CONTAINER(vbox), url_list_container);

  GtkWidget* scroll_window = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll_window),
                                 GTK_POLICY_NEVER,
                                 GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scroll_window),
                                      GTK_SHADOW_ETCHED_IN);
  gtk_container_add(GTK_CONTAINER(url_list_container),
                    scroll_window);
  startup_custom_pages_tree_ = gtk_tree_view_new();
  gtk_container_add(GTK_CONTAINER(scroll_window), startup_custom_pages_tree_);

  GtkWidget* url_list_buttons = gtk_vbox_new(FALSE, kOptionSpacing);
  gtk_box_pack_end(GTK_BOX(url_list_container), url_list_buttons,
                   FALSE, FALSE, 0);

  // TODO(mattm): fix mnemonics (see
  // MenuGtk::ConvertAcceleratorsFromWindowsStyle)
  startup_add_custom_page_button_ = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_OPTIONS_STARTUP_ADD_BUTTON).c_str());
  gtk_container_add(GTK_CONTAINER(url_list_buttons),
                    startup_add_custom_page_button_);
  startup_remove_custom_page_button_ = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_OPTIONS_STARTUP_REMOVE_BUTTON).c_str());
  gtk_container_add(GTK_CONTAINER(url_list_buttons),
                    startup_remove_custom_page_button_);
  startup_use_current_page_button_ = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_OPTIONS_STARTUP_USE_CURRENT).c_str());
  gtk_container_add(GTK_CONTAINER(url_list_buttons),
                    startup_use_current_page_button_);

  // TODO(mattm): hook up custom url list stuff

  return vbox;
}

GtkWidget* GeneralPageGtk::InitHomepageGroup() {
  GtkWidget* vbox = gtk_vbox_new(FALSE, kOptionSpacing);

  homepage_use_newtab_radio_ = gtk_radio_button_new_with_label(NULL,
      l10n_util::GetStringUTF8(IDS_OPTIONS_HOMEPAGE_USE_NEWTAB).c_str());
  g_signal_connect(G_OBJECT(homepage_use_newtab_radio_), "toggled",
                   G_CALLBACK(OnNewTabIsHomePageToggled), this);
  gtk_container_add(GTK_CONTAINER(vbox), homepage_use_newtab_radio_);

  GtkWidget* homepage_hbox = gtk_hbox_new(FALSE, kLabelSpacing);
  gtk_container_add(GTK_CONTAINER(vbox), homepage_hbox);

  homepage_use_url_radio_ = gtk_radio_button_new_with_label_from_widget(
      GTK_RADIO_BUTTON(homepage_use_newtab_radio_),
      l10n_util::GetStringUTF8(IDS_OPTIONS_HOMEPAGE_USE_URL).c_str());
  g_signal_connect(G_OBJECT(homepage_use_url_radio_), "toggled",
                   G_CALLBACK(OnNewTabIsHomePageToggled), this);
  gtk_box_pack_start(GTK_BOX(homepage_hbox), homepage_use_url_radio_,
                     FALSE, FALSE, 0);
  homepage_use_url_entry_ = gtk_entry_new();
  g_signal_connect(G_OBJECT(homepage_use_url_entry_), "changed",
                   G_CALLBACK(OnHomepageUseUrlEntryChanged), this);
  gtk_box_pack_start(GTK_BOX(homepage_hbox), homepage_use_url_entry_,
                     TRUE, TRUE, 0);

  homepage_show_home_button_checkbox_ = gtk_check_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_OPTIONS_HOMEPAGE_SHOW_BUTTON).c_str());
  g_signal_connect(G_OBJECT(homepage_show_home_button_checkbox_), "toggled",
                   G_CALLBACK(OnShowHomeButtonToggled), this);
  gtk_container_add(GTK_CONTAINER(vbox), homepage_show_home_button_checkbox_);

  return vbox;
}

GtkWidget* GeneralPageGtk::InitDefaultSearchGroup() {
  GtkWidget* hbox = gtk_hbox_new(FALSE, kOptionSpacing);

  // TODO(mattm): hook these up
  default_search_engine_combobox_ = gtk_combo_box_new();
  gtk_container_add(GTK_CONTAINER(hbox), default_search_engine_combobox_);

  default_search_manage_engines_button_ = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(
          IDS_OPTIONS_DEFAULTSEARCH_MANAGE_ENGINES_LINK).c_str());
  gtk_box_pack_end(GTK_BOX(hbox), default_search_manage_engines_button_,
                   FALSE, FALSE, 0);

  return hbox;
}

GtkWidget* GeneralPageGtk::InitDefaultBrowserGroup() {
  GtkWidget* vbox = gtk_vbox_new(FALSE, kOptionSpacing);

  default_browser_status_label_ = gtk_label_new(NULL);
  gtk_box_pack_start(GTK_BOX(vbox), default_browser_status_label_,
                     FALSE, FALSE, 0);

  default_browser_use_as_default_button_ = gtk_button_new_with_label(
      l10n_util::GetStringFUTF8(IDS_OPTIONS_DEFAULTBROWSER_USEASDEFAULT,
          l10n_util::GetStringUTF16(IDS_PRODUCT_NAME)).c_str());
  g_signal_connect(G_OBJECT(default_browser_use_as_default_button_), "clicked",
                   G_CALLBACK(OnBrowserUseAsDefaultClicked), this);
  gtk_box_pack_start(GTK_BOX(vbox), default_browser_use_as_default_button_,
                     FALSE, FALSE, 0);

  GtkWidget* vbox_alignment = gtk_alignment_new(0.0, 0.5, 0.0, 0.0);
  gtk_container_add(GTK_CONTAINER(vbox_alignment), vbox);

  SetDefaultBrowserUIState(ShellIntegration::IsDefaultBrowser());

  return vbox_alignment;
}

// static
void GeneralPageGtk::OnStartupRadioToggled(GtkToggleButton* toggle_button,
                                           GeneralPageGtk* general_page) {
  if (!gtk_toggle_button_get_active(toggle_button)) {
    // When selecting a radio button, we get two signals (one for the old radio
    // being toggled off, one for the new one being toggled on.)  Ignore the
    // signal for toggling off the old button.
    return;
  }
  general_page->SaveStartupPref();
  GtkWidget* sender = GTK_WIDGET(toggle_button);
  if (sender == general_page->startup_homepage_radio_) {
    general_page->UserMetricsRecordAction(L"Options_Startup_Homepage",
                                          general_page->profile()->GetPrefs());
  } else if (sender == general_page->startup_last_session_radio_) {
    general_page->UserMetricsRecordAction(L"Options_Startup_LastSession",
                                          general_page->profile()->GetPrefs());
  } else if (sender == general_page->startup_custom_radio_) {
    general_page->UserMetricsRecordAction(L"Options_Startup_Custom",
                                          general_page->profile()->GetPrefs());
  }
}

// static
void GeneralPageGtk::OnNewTabIsHomePageToggled(GtkToggleButton* toggle_button,
                                               GeneralPageGtk* general_page) {
  if (!gtk_toggle_button_get_active(toggle_button)) {
    // Ignore the signal for toggling off the old button.
    return;
  }
  GtkWidget* sender = GTK_WIDGET(toggle_button);
  if (sender == general_page->homepage_use_newtab_radio_) {
    general_page->SetHomepage(GURL());
    general_page->UserMetricsRecordAction(L"Options_Homepage_UseNewTab",
                                          general_page->profile()->GetPrefs());
    gtk_widget_set_sensitive(general_page->homepage_use_url_entry_, FALSE);
  } else if (sender == general_page->homepage_use_url_radio_) {
    general_page->SetHomepageFromEntry();
    general_page->UserMetricsRecordAction(L"Options_Homepage_UseURL",
                                          general_page->profile()->GetPrefs());
    gtk_widget_set_sensitive(general_page->homepage_use_url_entry_, TRUE);
  }
}

// static
void GeneralPageGtk::OnHomepageUseUrlEntryChanged(
    GtkEditable* editable,
    GeneralPageGtk* general_page) {
  general_page->SetHomepageFromEntry();
}

// static
void GeneralPageGtk::OnShowHomeButtonToggled(GtkToggleButton* toggle_button,
                                             GeneralPageGtk* general_page) {
  bool enabled = gtk_toggle_button_get_active(toggle_button);
  general_page->show_home_button_.SetValue(enabled);
  if (enabled) {
    general_page->UserMetricsRecordAction(L"Options_Homepage_ShowHomeButton",
                                          general_page->profile()->GetPrefs());
  } else {
    general_page->UserMetricsRecordAction(L"Options_Homepage_HideHomeButton",
                                          general_page->profile()->GetPrefs());
  }
}

// static
void GeneralPageGtk::OnBrowserUseAsDefaultClicked(
    GtkButton* button,
    GeneralPageGtk* general_page) {
  general_page->SetDefaultBrowserUIState(
      ShellIntegration::SetAsDefaultBrowser());
  // If the user made Chrome the default browser, then he/she arguably wants
  // to be notified when that changes.
  general_page->profile()->GetPrefs()->SetBoolean(prefs::kCheckDefaultBrowser,
                                                  true);
  general_page->UserMetricsRecordAction(L"Options_SetAsDefaultBrowser",
                                        general_page->profile()->GetPrefs());
}

void GeneralPageGtk::SaveStartupPref() {
  SessionStartupPref pref;

  if (gtk_toggle_button_get_active(
      GTK_TOGGLE_BUTTON(startup_last_session_radio_))) {
    pref.type = SessionStartupPref::LAST;
  } else if (gtk_toggle_button_get_active(
      GTK_TOGGLE_BUTTON(startup_custom_radio_))) {
    pref.type = SessionStartupPref::URLS;
  }

  // TODO(mattm): save custom URL list
  // pref.urls = startup_custom_pages_table_model_->GetURLs();

  SessionStartupPref::SetStartupPref(profile()->GetPrefs(), pref);
}

void GeneralPageGtk::SetHomepage(const GURL& homepage) {
  if (!homepage.is_valid() || homepage.spec() == chrome::kChromeUINewTabURL) {
    new_tab_page_is_home_page_.SetValue(true);
  } else {
    new_tab_page_is_home_page_.SetValue(false);
    homepage_.SetValue(UTF8ToWide(homepage.spec()));
  }
}

void GeneralPageGtk::SetHomepageFromEntry() {
  GURL url(URLFixerUpper::FixupURL(
      gtk_entry_get_text(GTK_ENTRY(homepage_use_url_entry_)), ""));
  SetHomepage(url);
}

void GeneralPageGtk::EnableCustomHomepagesControls(bool enable) {
  gtk_widget_set_sensitive(startup_add_custom_page_button_, enable);
  GtkTreeSelection* selection = gtk_tree_view_get_selection(
      GTK_TREE_VIEW(startup_custom_pages_tree_));
  gtk_widget_set_sensitive(startup_remove_custom_page_button_,
      enable && gtk_tree_selection_count_selected_rows(selection));
  gtk_widget_set_sensitive(startup_use_current_page_button_, enable);
  gtk_widget_set_sensitive(startup_custom_pages_tree_, enable);
}

void GeneralPageGtk::SetDefaultBrowserUIState(bool is_default) {
  const char* color;
  std::string text;
  if (is_default) {
    color = kDefaultBrowserLabelColor;
    text = l10n_util::GetStringFUTF8(IDS_OPTIONS_DEFAULTBROWSER_DEFAULT,
        l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));
  } else {
    color = kNotDefaultBrowserLabelColor;
    text = l10n_util::GetStringFUTF8(IDS_OPTIONS_DEFAULTBROWSER_NOTDEFAULT,
        l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));
  }
  char* markup = g_markup_printf_escaped(kDefaultBrowserLabelMarkup,
                                         color, text.c_str());
  gtk_label_set_markup(GTK_LABEL(default_browser_status_label_), markup);
  g_free(markup);

  gtk_widget_set_sensitive(default_browser_use_as_default_button_, !is_default);
}
