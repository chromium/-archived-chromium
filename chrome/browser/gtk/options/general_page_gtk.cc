// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/options/general_page_gtk.h"

#include "app/l10n_util.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/gtk/keyword_editor_view.h"
#include "chrome/browser/gtk/list_store_favicon_loader.h"
#include "chrome/browser/gtk/options/options_layout_gtk.h"
#include "chrome/browser/gtk/options/url_picker_dialog_gtk.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/session_startup_pref.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/gtk_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/url_constants.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"

namespace {

// Markup for the text showing the current state of the default browser
const char kDefaultBrowserLabelMarkup[] = "<span color='#%s'>%s</span>";

// Color of the default browser text when Chromium is the default browser
const char kDefaultBrowserLabelColor[] = "008700";

// Color of the default browser text when Chromium is not the default browser
const char kNotDefaultBrowserLabelColor[] = "870000";

// Column ids for |startup_custom_pages_model_|.
enum {
  COL_FAVICON_HANDLE,
  COL_FAVICON,
  COL_URL,
  COL_COUNT,
};

// Column ids for |default_search_engines_model_|.
enum {
  SEARCH_ENGINES_COL_INDEX,
  SEARCH_ENGINES_COL_TITLE,
  SEARCH_ENGINES_COL_COUNT,
};

}

///////////////////////////////////////////////////////////////////////////////
// GeneralPageGtk, public:

GeneralPageGtk::GeneralPageGtk(Profile* profile)
    : OptionsPageBase(profile),
      template_url_model_(NULL),
      default_search_initializing_(true),
      initializing_(true)  {
  OptionsLayoutBuilderGtk options_builder;
  options_builder.AddOptionGroup(
      l10n_util::GetStringUTF8(IDS_OPTIONS_STARTUP_GROUP_NAME),
      InitStartupGroup(), true);
  options_builder.AddOptionGroup(
      l10n_util::GetStringUTF8(IDS_OPTIONS_HOMEPAGE_GROUP_NAME),
      InitHomepageGroup(), false);
  options_builder.AddOptionGroup(
      l10n_util::GetStringUTF8(IDS_OPTIONS_DEFAULTSEARCH_GROUP_NAME),
      InitDefaultSearchGroup(), false);
  options_builder.AddOptionGroup(
      l10n_util::GetStringUTF8(IDS_OPTIONS_DEFAULTBROWSER_GROUP_NAME),
      InitDefaultBrowserGroup(), false);
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

  if (template_url_model_)
    template_url_model_->RemoveObserver(this);
}

///////////////////////////////////////////////////////////////////////////////
// GeneralPageGtk, OptionsPageBase overrides:

void GeneralPageGtk::NotifyPrefChanged(const std::wstring* pref_name) {
  initializing_ = true;
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

  if (!pref_name || *pref_name == prefs::kURLsToRestoreOnStartup) {
    PrefService* prefs = profile()->GetPrefs();
    const SessionStartupPref startup_pref =
        SessionStartupPref::GetStartupPref(prefs);
    PopulateCustomUrlList(startup_pref.urls);
  }

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
  initializing_ = false;
}

void GeneralPageGtk::HighlightGroup(OptionsGroup highlight_group) {
  // TODO(mattm): implement group highlighting
}

///////////////////////////////////////////////////////////////////////////////
// GeneralPageGtk, private:

GtkWidget* GeneralPageGtk::InitStartupGroup() {
  GtkWidget* vbox = gtk_vbox_new(FALSE, gtk_util::kControlSpacing);

  startup_homepage_radio_ = gtk_radio_button_new_with_label(NULL,
      l10n_util::GetStringUTF8(
          IDS_OPTIONS_STARTUP_SHOW_DEFAULT_AND_NEWTAB).c_str());
  g_signal_connect(G_OBJECT(startup_homepage_radio_), "toggled",
                   G_CALLBACK(OnStartupRadioToggled), this);
  gtk_box_pack_start(GTK_BOX(vbox), startup_homepage_radio_, FALSE, FALSE, 0);

  startup_last_session_radio_ = gtk_radio_button_new_with_label_from_widget(
      GTK_RADIO_BUTTON(startup_homepage_radio_),
      l10n_util::GetStringUTF8(IDS_OPTIONS_STARTUP_SHOW_LAST_SESSION).c_str());
  g_signal_connect(G_OBJECT(startup_last_session_radio_), "toggled",
                   G_CALLBACK(OnStartupRadioToggled), this);
  gtk_box_pack_start(GTK_BOX(vbox), startup_last_session_radio_,
                     FALSE, FALSE, 0);

  startup_custom_radio_ = gtk_radio_button_new_with_label_from_widget(
      GTK_RADIO_BUTTON(startup_homepage_radio_),
      l10n_util::GetStringUTF8(IDS_OPTIONS_STARTUP_SHOW_PAGES).c_str());
  g_signal_connect(G_OBJECT(startup_custom_radio_), "toggled",
                   G_CALLBACK(OnStartupRadioToggled), this);
  gtk_box_pack_start(GTK_BOX(vbox), startup_custom_radio_, FALSE, FALSE, 0);

  GtkWidget* url_list_container = gtk_hbox_new(FALSE,
                                               gtk_util::kControlSpacing);
  gtk_box_pack_start(GTK_BOX(vbox), url_list_container, TRUE, TRUE, 0);

  GtkWidget* scroll_window = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll_window),
                                 GTK_POLICY_AUTOMATIC,
                                 GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scroll_window),
                                      GTK_SHADOW_ETCHED_IN);
  gtk_container_add(GTK_CONTAINER(url_list_container),
                    scroll_window);
  startup_custom_pages_model_ = gtk_list_store_new(COL_COUNT,
                                                   G_TYPE_INT,
                                                   GDK_TYPE_PIXBUF,
                                                   G_TYPE_STRING);
  startup_custom_pages_tree_ = gtk_tree_view_new_with_model(
      GTK_TREE_MODEL(startup_custom_pages_model_));
  gtk_container_add(GTK_CONTAINER(scroll_window), startup_custom_pages_tree_);

  gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(startup_custom_pages_tree_),
                                    FALSE);
  GtkTreeViewColumn* column = gtk_tree_view_column_new();
  GtkCellRenderer* renderer = gtk_cell_renderer_pixbuf_new();
  gtk_tree_view_column_pack_start(column, renderer, FALSE);
  gtk_tree_view_column_add_attribute(column, renderer, "pixbuf", COL_FAVICON);
  renderer = gtk_cell_renderer_text_new();
  gtk_tree_view_column_pack_start(column, renderer, TRUE);
  gtk_tree_view_column_add_attribute(column, renderer, "text", COL_URL);
  gtk_tree_view_append_column(GTK_TREE_VIEW(startup_custom_pages_tree_),
                              column);
  startup_custom_pages_selection_ = gtk_tree_view_get_selection(
      GTK_TREE_VIEW(startup_custom_pages_tree_));
  gtk_tree_selection_set_mode(startup_custom_pages_selection_,
                              GTK_SELECTION_MULTIPLE);
  g_signal_connect(G_OBJECT(startup_custom_pages_selection_), "changed",
                   G_CALLBACK(OnStartupPagesSelectionChanged), this);
  favicon_loader_.reset(new ListStoreFavIconLoader(startup_custom_pages_model_,
                                                   COL_FAVICON,
                                                   COL_FAVICON_HANDLE,
                                                   profile(),
                                                   &fav_icon_consumer_));

  GtkWidget* url_list_buttons = gtk_vbox_new(FALSE, gtk_util::kControlSpacing);
  gtk_box_pack_end(GTK_BOX(url_list_container), url_list_buttons,
                   FALSE, FALSE, 0);

  startup_add_custom_page_button_ = gtk_button_new_with_mnemonic(
      gtk_util::ConvertAcceleratorsFromWindowsStyle(
          l10n_util::GetStringUTF8(IDS_OPTIONS_STARTUP_ADD_BUTTON)).c_str());
  g_signal_connect(G_OBJECT(startup_add_custom_page_button_), "clicked",
                   G_CALLBACK(OnStartupAddCustomPageClicked), this);
  gtk_box_pack_start(GTK_BOX(url_list_buttons), startup_add_custom_page_button_,
                     FALSE, FALSE, 0);
  startup_remove_custom_page_button_ = gtk_button_new_with_mnemonic(
      gtk_util::ConvertAcceleratorsFromWindowsStyle(
        l10n_util::GetStringUTF8(IDS_OPTIONS_STARTUP_REMOVE_BUTTON)).c_str());
  g_signal_connect(G_OBJECT(startup_remove_custom_page_button_), "clicked",
                   G_CALLBACK(OnStartupRemoveCustomPageClicked), this);
  gtk_box_pack_start(GTK_BOX(url_list_buttons),
                     startup_remove_custom_page_button_, FALSE, FALSE, 0);
  startup_use_current_page_button_ = gtk_button_new_with_mnemonic(
      gtk_util::ConvertAcceleratorsFromWindowsStyle(
          l10n_util::GetStringUTF8(IDS_OPTIONS_STARTUP_USE_CURRENT)).c_str());
  g_signal_connect(G_OBJECT(startup_use_current_page_button_), "clicked",
                   G_CALLBACK(OnStartupUseCurrentPageClicked), this);
  gtk_box_pack_start(GTK_BOX(url_list_buttons),
                     startup_use_current_page_button_, FALSE, FALSE, 0);

  return vbox;
}

GtkWidget* GeneralPageGtk::InitHomepageGroup() {
  GtkWidget* vbox = gtk_vbox_new(FALSE, gtk_util::kControlSpacing);

  homepage_use_newtab_radio_ = gtk_radio_button_new_with_label(NULL,
      l10n_util::GetStringUTF8(IDS_OPTIONS_HOMEPAGE_USE_NEWTAB).c_str());
  g_signal_connect(G_OBJECT(homepage_use_newtab_radio_), "toggled",
                   G_CALLBACK(OnNewTabIsHomePageToggled), this);
  gtk_container_add(GTK_CONTAINER(vbox), homepage_use_newtab_radio_);

  GtkWidget* homepage_hbox = gtk_hbox_new(FALSE, gtk_util::kLabelSpacing);
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
  GtkWidget* hbox = gtk_hbox_new(FALSE, gtk_util::kControlSpacing);

  default_search_engines_model_ = gtk_list_store_new(SEARCH_ENGINES_COL_COUNT,
                                                     G_TYPE_UINT,
                                                     G_TYPE_STRING);
  default_search_engine_combobox_ = gtk_combo_box_new_with_model(
      GTK_TREE_MODEL(default_search_engines_model_));
  g_signal_connect(G_OBJECT(default_search_engine_combobox_), "changed",
                   G_CALLBACK(OnDefaultSearchEngineChanged), this);
  gtk_container_add(GTK_CONTAINER(hbox), default_search_engine_combobox_);

  GtkCellRenderer* renderer = gtk_cell_renderer_text_new();
  gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(default_search_engine_combobox_),
                             renderer, TRUE);
  gtk_cell_layout_set_attributes(
      GTK_CELL_LAYOUT(default_search_engine_combobox_), renderer,
      "text", SEARCH_ENGINES_COL_TITLE,
      NULL);

  template_url_model_ = profile()->GetTemplateURLModel();
  if (template_url_model_) {
    template_url_model_->Load();
    template_url_model_->AddObserver(this);
  }
  OnTemplateURLModelChanged();

  default_search_manage_engines_button_ = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(
          IDS_OPTIONS_DEFAULTSEARCH_MANAGE_ENGINES_LINK).c_str());
  g_signal_connect(G_OBJECT(default_search_manage_engines_button_), "clicked",
                   G_CALLBACK(OnDefaultSearchManageEnginesClicked), this);
  gtk_box_pack_end(GTK_BOX(hbox), default_search_manage_engines_button_,
                   FALSE, FALSE, 0);

  return hbox;
}

GtkWidget* GeneralPageGtk::InitDefaultBrowserGroup() {
  GtkWidget* vbox = gtk_vbox_new(FALSE, gtk_util::kControlSpacing);

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
  if (general_page->initializing_)
    return;
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
void GeneralPageGtk::OnStartupAddCustomPageClicked(
    GtkButton* button, GeneralPageGtk* general_page) {
  new UrlPickerDialogGtk(
      NewCallback(general_page, &GeneralPageGtk::OnAddCustomUrl),
      general_page->profile(),
      GTK_WINDOW(gtk_widget_get_toplevel(general_page->page_)));
}

// static
void GeneralPageGtk::OnStartupRemoveCustomPageClicked(
    GtkButton* button, GeneralPageGtk* general_page) {
  general_page->RemoveSelectedCustomUrls();
}

// static
void GeneralPageGtk::OnStartupUseCurrentPageClicked(
    GtkButton* button, GeneralPageGtk* general_page) {
  general_page->SetCustomUrlListFromCurrentPages();
}

// static
void GeneralPageGtk::OnStartupPagesSelectionChanged(
    GtkTreeSelection *selection, GeneralPageGtk* general_page) {
  general_page->EnableCustomHomepagesControls(true);
}

// static
void GeneralPageGtk::OnNewTabIsHomePageToggled(GtkToggleButton* toggle_button,
                                               GeneralPageGtk* general_page) {
  if (general_page->initializing_)
    return;
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
  if (general_page->initializing_)
    return;
  general_page->SetHomepageFromEntry();
}

// static
void GeneralPageGtk::OnShowHomeButtonToggled(GtkToggleButton* toggle_button,
                                             GeneralPageGtk* general_page) {
  if (general_page->initializing_)
    return;
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
void GeneralPageGtk::OnDefaultSearchEngineChanged(
    GtkComboBox* combo_box,
    GeneralPageGtk* general_page) {
  if (general_page->default_search_initializing_)
    return;
  general_page->SetDefaultSearchEngineFromComboBox();
}

// static
void GeneralPageGtk::OnDefaultSearchManageEnginesClicked(
    GtkButton* button, GeneralPageGtk* general_page) {
  KeywordEditorView::Show(general_page->profile());
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

  pref.urls = GetCustomUrlList();

  SessionStartupPref::SetStartupPref(profile()->GetPrefs(), pref);
}

void GeneralPageGtk::PopulateCustomUrlList(const std::vector<GURL>& urls) {
  gtk_list_store_clear(startup_custom_pages_model_);
  for (size_t i = 0; i < urls.size(); ++i) {
    GtkTreeIter iter;
    gtk_list_store_append(startup_custom_pages_model_, &iter);
    PopulateCustomUrlRow(urls[i], &iter);
  }
}

void GeneralPageGtk::PopulateCustomUrlRow(const GURL& url, GtkTreeIter* iter) {
  favicon_loader_->LoadFaviconForRow(url, iter);
  gtk_list_store_set(startup_custom_pages_model_, iter,
                     COL_URL, url.spec().c_str(),
                     -1);
}

void GeneralPageGtk::SetCustomUrlListFromCurrentPages() {
  std::vector<GURL> urls;
  for (BrowserList::const_iterator browser_i = BrowserList::begin();
       browser_i != BrowserList::end(); ++browser_i) {
    Browser* browser = *browser_i;
    if (browser->profile() != profile())
      continue;  // Only want entries for open profile.

    for (int tab_index = 0; tab_index < browser->tab_count(); ++tab_index) {
      TabContents* tab = browser->GetTabContentsAt(tab_index);
      if (tab->ShouldDisplayURL()) {
        const GURL url = browser->GetTabContentsAt(tab_index)->GetURL();
        if (!url.is_empty())
          urls.push_back(url);
      }
    }
  }
  PopulateCustomUrlList(urls);
  SaveStartupPref();
}

void GeneralPageGtk::OnAddCustomUrl(const GURL& url) {
  GtkTreeIter iter;
  if (gtk_tree_selection_count_selected_rows(
      startup_custom_pages_selection_) == 1) {
    GList* list = gtk_tree_selection_get_selected_rows(
        startup_custom_pages_selection_, NULL);
    GtkTreeIter sibling;
    gtk_tree_model_get_iter(GTK_TREE_MODEL(startup_custom_pages_model_),
                            &sibling, static_cast<GtkTreePath*>(list->data));
    g_list_foreach(list, (GFunc)gtk_tree_path_free, NULL);
    g_list_free(list);

    gtk_list_store_insert_before(startup_custom_pages_model_,
                                 &iter, &sibling);
  } else {
    gtk_list_store_append(startup_custom_pages_model_, &iter);
  }
  PopulateCustomUrlRow(url, &iter);
  SaveStartupPref();
}

void GeneralPageGtk::RemoveSelectedCustomUrls() {
  GList* list = gtk_tree_selection_get_selected_rows(
      startup_custom_pages_selection_, NULL);
  std::vector<GtkTreeIter> selected_iters(
      gtk_tree_selection_count_selected_rows(startup_custom_pages_selection_));
  GList* node;
  size_t i;
  for (i = 0, node = list; node != NULL; ++i, node = node->next) {
    gtk_tree_model_get_iter(GTK_TREE_MODEL(startup_custom_pages_model_),
                            &selected_iters[i],
                            static_cast<GtkTreePath*>(node->data));
  }
  g_list_foreach(list, (GFunc)gtk_tree_path_free, NULL);
  g_list_free(list);

  for (i = 0; i < selected_iters.size(); ++i) {
    gtk_list_store_remove(startup_custom_pages_model_, &selected_iters[i]);
  }
  SaveStartupPref();
}

std::vector<GURL> GeneralPageGtk::GetCustomUrlList() const {
  std::vector<GURL> urls;
  GtkTreeIter iter;
  gboolean valid = gtk_tree_model_get_iter_first(
      GTK_TREE_MODEL(startup_custom_pages_model_), &iter);
  while (valid) {
    gchar* url_data;
    gtk_tree_model_get(GTK_TREE_MODEL(startup_custom_pages_model_), &iter,
                       COL_URL, &url_data,
                       -1);
    urls.push_back(GURL(url_data));
    g_free(url_data);
    valid = gtk_tree_model_iter_next(
        GTK_TREE_MODEL(startup_custom_pages_model_), &iter);
  }
  return urls;
}

void GeneralPageGtk::OnTemplateURLModelChanged() {
  if (!template_url_model_ || !template_url_model_->loaded()) {
    EnableDefaultSearchEngineComboBox(false);
    return;
  }
  default_search_initializing_ = true;
  gtk_list_store_clear(default_search_engines_model_);
  const TemplateURL* default_search_provider =
      template_url_model_->GetDefaultSearchProvider();
  std::vector<const TemplateURL*> model_urls =
      template_url_model_->GetTemplateURLs();
  bool populated = false;
  for (size_t i = 0; i < model_urls.size(); ++i) {
    if (!model_urls[i]->ShowInDefaultList())
      continue;
    populated = true;
    GtkTreeIter iter;
    gtk_list_store_append(default_search_engines_model_, &iter);
    gtk_list_store_set(
        default_search_engines_model_, &iter,
        SEARCH_ENGINES_COL_INDEX, i,
        SEARCH_ENGINES_COL_TITLE,
        WideToUTF8(model_urls[i]->short_name()).c_str(),
        -1);
    if (model_urls[i] == default_search_provider) {
      gtk_combo_box_set_active_iter(
          GTK_COMBO_BOX(default_search_engine_combobox_), &iter);
    }
  }
  EnableDefaultSearchEngineComboBox(populated);
  default_search_initializing_ = false;
}

void GeneralPageGtk::SetDefaultSearchEngineFromComboBox() {
  GtkTreeIter iter;
  if (!gtk_combo_box_get_active_iter(
      GTK_COMBO_BOX(default_search_engine_combobox_), &iter)) {
    return;
  }
  guint index;
  gtk_tree_model_get(GTK_TREE_MODEL(default_search_engines_model_), &iter,
                     SEARCH_ENGINES_COL_INDEX, &index,
                     -1);
  std::vector<const TemplateURL*> model_urls =
      template_url_model_->GetTemplateURLs();
  if (index < model_urls.size())
    template_url_model_->SetDefaultSearchProvider(model_urls[index]);
  else
    NOTREACHED();
}

void GeneralPageGtk::EnableDefaultSearchEngineComboBox(bool enable) {
  gtk_widget_set_sensitive(default_search_engine_combobox_, enable);
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
  gtk_widget_set_sensitive(startup_remove_custom_page_button_,
      enable &&
      gtk_tree_selection_count_selected_rows(startup_custom_pages_selection_));
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
