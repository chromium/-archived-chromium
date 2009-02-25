// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/options/general_page_view.h"

#include "base/gfx/png_decoder.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/thread.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/dom_ui/new_tab_ui.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/session_startup_pref.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/views/keyword_editor_view.h"
#include "chrome/browser/views/options/options_group_view.h"
#include "chrome/browser/views/standard_layout.h"
#include "chrome/browser/dom_ui/new_tab_ui.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/views/checkbox.h"
#include "chrome/views/grid_layout.h"
#include "chrome/views/label.h"
#include "chrome/views/radio_button.h"
#include "chrome/views/table_view.h"
#include "chrome/views/text_field.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "skia/include/SkBitmap.h"

static const int kStartupRadioGroup = 1;
static const int kHomePageRadioGroup = 2;
static const SkColor kDefaultBrowserLabelColor = SkColorSetRGB(0, 135, 0);
static const SkColor kNotDefaultBrowserLabelColor = SkColorSetRGB(135, 0, 0);

namespace {
std::wstring GetNewTabUIURLString() {
  return UTF8ToWide(NewTabUI::GetBaseURL().spec());
}
}

///////////////////////////////////////////////////////////////////////////////
// GeneralPageView::DefaultBrowserWorker
//
//  A helper object that handles checking if Chrome is the default browser on
//  Windows and also setting it as the default browser. These operations are
//  performed asynchronously on the file thread since registry access is
//  involved and this can be slow.
//
class GeneralPageView::DefaultBrowserWorker
    : public base::RefCountedThreadSafe<GeneralPageView::DefaultBrowserWorker> {
 public:
  explicit DefaultBrowserWorker(GeneralPageView* general_page_view);

  // Checks if Chrome is the default browser.
  void StartCheckDefaultBrowser();

  // Sets Chrome as the default browser.
  void StartSetAsDefaultBrowser();

  // Called to notify the worker that the view is gone.
  void ViewDestroyed();

 private:
  // Functions that track the process of checking if Chrome is the default
  // browser.
  // |ExecuteCheckDefaultBrowser| checks the registry on the file thread.
  // |CompleteCheckDefaultBrowser| notifies the view to update on the UI thread.
  void ExecuteCheckDefaultBrowser();
  void CompleteCheckDefaultBrowser(bool is_default);

  // Functions that track the process of setting Chrome as the default browser.
  // |ExecuteSetAsDefaultBrowser| updates the registry on the file thread.
  // |CompleteSetAsDefaultBrowser| notifies the view to update on the UI thread.
  void ExecuteSetAsDefaultBrowser();
  void CompleteSetAsDefaultBrowser();

  // Updates the UI in our associated view with the current default browser
  // state.
  void UpdateUI(bool is_default);

  GeneralPageView* general_page_view_;

  MessageLoop* ui_loop_;
  MessageLoop* file_loop_;

  DISALLOW_EVIL_CONSTRUCTORS(GeneralPageView::DefaultBrowserWorker);
};

GeneralPageView::DefaultBrowserWorker::DefaultBrowserWorker(
    GeneralPageView* general_page_view)
    : general_page_view_(general_page_view),
      ui_loop_(MessageLoop::current()),
      file_loop_(g_browser_process->file_thread()->message_loop()) {
}

void GeneralPageView::DefaultBrowserWorker::StartCheckDefaultBrowser() {
  general_page_view_->SetDefaultBrowserUIState(STATE_PROCESSING);
  file_loop_->PostTask(FROM_HERE, NewRunnableMethod(this,
      &DefaultBrowserWorker::ExecuteCheckDefaultBrowser));
}

void GeneralPageView::DefaultBrowserWorker::StartSetAsDefaultBrowser() {
  general_page_view_->SetDefaultBrowserUIState(STATE_PROCESSING);
  file_loop_->PostTask(FROM_HERE, NewRunnableMethod(this,
      &DefaultBrowserWorker::ExecuteSetAsDefaultBrowser));
}

void GeneralPageView::DefaultBrowserWorker::ViewDestroyed() {
  // Our associated view has gone away, so we shouldn't call back to it if
  // our worker thread returns after the view is dead.
  general_page_view_ = NULL;
}

///////////////////////////////////////////////////////////////////////////////
// DefaultBrowserWorker, private:

void GeneralPageView::DefaultBrowserWorker::ExecuteCheckDefaultBrowser() {
  DCHECK(MessageLoop::current() == file_loop_);
  bool is_default = ShellIntegration::IsDefaultBrowser();
  ui_loop_->PostTask(FROM_HERE, NewRunnableMethod(this,
      &DefaultBrowserWorker::CompleteCheckDefaultBrowser, is_default));
}

void GeneralPageView::DefaultBrowserWorker::CompleteCheckDefaultBrowser(
    bool is_default) {
  DCHECK(MessageLoop::current() == ui_loop_);
  UpdateUI(is_default);
}

void GeneralPageView::DefaultBrowserWorker::ExecuteSetAsDefaultBrowser() {
  DCHECK(MessageLoop::current() == file_loop_);
  bool result = ShellIntegration::SetAsDefaultBrowser();
  ui_loop_->PostTask(FROM_HERE, NewRunnableMethod(this,
      &DefaultBrowserWorker::CompleteSetAsDefaultBrowser));
}

void GeneralPageView::DefaultBrowserWorker::CompleteSetAsDefaultBrowser() {
  DCHECK(MessageLoop::current() == ui_loop_);
  if (general_page_view_) {
    // Set as default completed, check again to make sure it stuck...
    StartCheckDefaultBrowser();
  }
}

void GeneralPageView::DefaultBrowserWorker::UpdateUI(bool is_default) {
  if (general_page_view_) {
    DefaultBrowserUIState state =
        is_default ? STATE_DEFAULT : STATE_NOT_DEFAULT;
    general_page_view_->SetDefaultBrowserUIState(state);
  }
}

///////////////////////////////////////////////////////////////////////////////
// CustomHomePagesTableModel

// CustomHomePagesTableModel is the model for the TableView showing the list
// of pages the user wants opened on startup.

class CustomHomePagesTableModel : public views::TableModel {
 public:
  explicit CustomHomePagesTableModel(Profile* profile);
  virtual ~CustomHomePagesTableModel() {}

  // Sets the set of urls that this model contains.
  void SetURLs(const std::vector<GURL>& urls);

  // Adds an entry at the specified index.
  void Add(int index, const GURL& url);

  // Removes the entry at the specified index.
  void Remove(int index);

  // Returns the set of urls this model contains.
  std::vector<GURL> GetURLs();

  // views::TableModel overrides:
  virtual int RowCount();
  virtual std::wstring GetText(int row, int column_id);
  virtual SkBitmap GetIcon(int row);
  virtual void SetObserver(views::TableModelObserver* observer);

 private:
  // Each item in the model is represented as an Entry. Entry stores the URL
  // and favicon of the page.
  struct Entry {
    Entry() : fav_icon_handle(0) {}

    // URL of the page.
    GURL url;

    // Icon for the page.
    SkBitmap icon;

    // If non-zero, indicates we're loading the favicon for the page.
    HistoryService::Handle fav_icon_handle;
  };

  static void InitClass();

  // Loads the favicon for the specified entry.
  void LoadFavIcon(Entry* entry);

  // Callback from history service. Updates the icon of the Entry whose
  // fav_icon_handle matches handle and notifies the observer of the change.
  void OnGotFavIcon(HistoryService::Handle handle,
                    bool know_fav_icon,
                    scoped_refptr<RefCountedBytes> image_data,
                    bool is_expired,
                    GURL icon_url);

  // Returns the entry whose fav_icon_handle matches handle and sets entry_index
  // to the index of the entry.
  Entry* GetEntryByLoadHandle(HistoryService::Handle handle, int* entry_index);

  // Set of entries we're showing.
  std::vector<Entry> entries_;

  // Default icon to show when one can't be found for the URL.
  static SkBitmap default_favicon_;

  // Profile used to load icons.
  Profile* profile_;

  views::TableModelObserver* observer_;

  // Used in loading favicons.
  CancelableRequestConsumer fav_icon_consumer_;

  DISALLOW_EVIL_CONSTRUCTORS(CustomHomePagesTableModel);
};

// static
SkBitmap CustomHomePagesTableModel::default_favicon_;

CustomHomePagesTableModel::CustomHomePagesTableModel(Profile* profile)
    : profile_(profile),
      observer_(NULL) {
  InitClass();
}

void CustomHomePagesTableModel::SetURLs(const std::vector<GURL>& urls) {
  entries_.resize(urls.size());
  for (size_t i = 0; i < urls.size(); ++i) {
    entries_[i].url = urls[i];
    LoadFavIcon(&(entries_[i]));
  }
  // Complete change, so tell the view to just rebuild itself.
  if (observer_)
    observer_->OnModelChanged();
}

void CustomHomePagesTableModel::Add(int index, const GURL& url) {
  DCHECK(index >= 0 && index <= RowCount());
  entries_.insert(entries_.begin() + static_cast<size_t>(index), Entry());
  entries_[index].url = url;
  LoadFavIcon(&(entries_[index]));
  if (observer_)
    observer_->OnItemsAdded(index, 1);
}

void CustomHomePagesTableModel::Remove(int index) {
  DCHECK(index >= 0 && index < RowCount());
  Entry* entry = &(entries_[index]);
  if (entry->fav_icon_handle) {
    // Pending load request, cancel it now so we don't deref a bogus pointer
    // when we get loaded notification.
    HistoryService* history =
        profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
    if (history)
      history->CancelRequest(entry->fav_icon_handle);
  }
  entries_.erase(entries_.begin() + static_cast<size_t>(index));
  if (observer_)
    observer_->OnItemsRemoved(index, 1);
}

std::vector<GURL> CustomHomePagesTableModel::GetURLs() {
  std::vector<GURL> urls(entries_.size());
  for (size_t i = 0; i < entries_.size(); ++i)
    urls[i] = entries_[i].url;
  return urls;
}

int CustomHomePagesTableModel::RowCount() {
  return static_cast<int>(entries_.size());
}

std::wstring CustomHomePagesTableModel::GetText(int row, int column_id) {
  DCHECK(column_id == 0);
  DCHECK(row >= 0 && row < RowCount());
  // No need to force URL to have LTR directionality because the custom home
  // pages control is created using LTR directionality.
  return UTF8ToWide(entries_[row].url.spec());
}

SkBitmap CustomHomePagesTableModel::GetIcon(int row) {
  DCHECK(row >= 0 && row < RowCount());
  if (!entries_[row].icon.isNull())
    return entries_[row].icon;
  return default_favicon_;
}

void CustomHomePagesTableModel::SetObserver(
    views::TableModelObserver* observer) {
  observer_ = observer;
}

void CustomHomePagesTableModel::InitClass() {
  static bool initialized = false;
  if (!initialized) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    default_favicon_ = *rb.GetBitmapNamed(IDR_DEFAULT_FAVICON);
    initialized = true;
  }
}

void CustomHomePagesTableModel::LoadFavIcon(Entry* entry) {
  HistoryService* history =
      profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  if (!history)
    return;
  entry->fav_icon_handle = history->GetFavIconForURL(
      entry->url, &fav_icon_consumer_,
      NewCallback(this, &CustomHomePagesTableModel::OnGotFavIcon));
}

void CustomHomePagesTableModel::OnGotFavIcon(
    HistoryService::Handle handle,
    bool know_fav_icon,
    scoped_refptr<RefCountedBytes> image_data,
    bool is_expired,
    GURL icon_url) {
  int entry_index;
  Entry* entry = GetEntryByLoadHandle(handle, &entry_index);
  DCHECK(entry);
  entry->fav_icon_handle = 0;
  if (know_fav_icon && image_data.get() && !image_data->data.empty()) {
    int width, height;
    std::vector<unsigned char> decoded_data;
    if (PNGDecoder::Decode(&image_data->data.front(), image_data->data.size(),
                           PNGDecoder::FORMAT_BGRA, &decoded_data, &width,
                           &height)) {
      entry->icon.setConfig(SkBitmap::kARGB_8888_Config, width, height);
      entry->icon.allocPixels();
      memcpy(entry->icon.getPixels(), &decoded_data.front(),
             width * height * 4);
      if (observer_)
        observer_->OnItemsChanged(static_cast<int>(entry_index), 1);
    }
  }
}

CustomHomePagesTableModel::Entry*
    CustomHomePagesTableModel::GetEntryByLoadHandle(
    HistoryService::Handle handle,
    int* index) {
  for (size_t i = 0; i < entries_.size(); ++i) {
    if (entries_[i].fav_icon_handle == handle) {
      *index = static_cast<int>(i);
      return &entries_[i];
    }
  }
  return NULL;
}

///////////////////////////////////////////////////////////////////////////////
// SearchEngineListModel

class SearchEngineListModel : public views::ComboBox::Model,
                              public TemplateURLModelObserver {
 public:
  explicit SearchEngineListModel(Profile* profile);
  virtual ~SearchEngineListModel();

  // Sets the ComboBox. SearchEngineListModel needs a handle to the ComboBox
  // so that when the TemplateURLModel changes the combobox can be updated.
  void SetComboBox(views::ComboBox* combo_box);

  // views::ComboBox::Model overrides:
  virtual int GetItemCount(views::ComboBox* source);
  virtual std::wstring GetItemAt(views::ComboBox* source, int index);

  // Returns the TemplateURL at the specified index.
  const TemplateURL* GetTemplateURLAt(int index);

  TemplateURLModel* model() { return template_url_model_; }

 private:
  // TemplateURLModelObserver methods.
  virtual void OnTemplateURLModelChanged();

  // Recalculates the TemplateURLs to display and notifies the combobox.
  void ResetContents();

  // Resets the selection of the combobox based on the users selected search
  // engine.
  void ChangeComboBoxSelection();

  TemplateURLModel* template_url_model_;

  // The combobox hosting us.
  views::ComboBox* combo_box_;

  // The TemplateURLs we're showing.
  typedef std::vector<const TemplateURL*> TemplateURLs;
  TemplateURLs template_urls_;

  DISALLOW_EVIL_CONSTRUCTORS(SearchEngineListModel);
};

SearchEngineListModel::SearchEngineListModel(Profile* profile)
    : template_url_model_(profile->GetTemplateURLModel()),
      combo_box_(NULL) {
  if (template_url_model_) {
    template_url_model_->Load();
    template_url_model_->AddObserver(this);
  }
  ResetContents();
}

SearchEngineListModel::~SearchEngineListModel() {
  if (template_url_model_)
    template_url_model_->RemoveObserver(this);
}

void SearchEngineListModel::SetComboBox(views::ComboBox* combo_box) {
  combo_box_ = combo_box;
  if (template_url_model_ && template_url_model_->loaded())
    ChangeComboBoxSelection();
  else
    combo_box_->SetEnabled(false);
}

int SearchEngineListModel::GetItemCount(views::ComboBox* source) {
  return static_cast<int>(template_urls_.size());
}

std::wstring SearchEngineListModel::GetItemAt(views::ComboBox* source,
                                              int index) {
  DCHECK(index < GetItemCount(combo_box_));
  return template_urls_[index]->short_name();
}

const TemplateURL* SearchEngineListModel::GetTemplateURLAt(int index) {
  DCHECK(index >= 0 && index < static_cast<int>(template_urls_.size()));
  return template_urls_[static_cast<int>(index)];
}

void SearchEngineListModel::OnTemplateURLModelChanged() {
  ResetContents();
}

void SearchEngineListModel::ResetContents() {
  if (!template_url_model_ || !template_url_model_->loaded())
    return;
  template_urls_.clear();
  TemplateURLs model_urls = template_url_model_->GetTemplateURLs();
  for (size_t i = 0; i < model_urls.size(); ++i) {
    if (model_urls[i]->ShowInDefaultList())
      template_urls_.push_back(model_urls[i]);
  }

  if (combo_box_) {
    combo_box_->ModelChanged();
    ChangeComboBoxSelection();
  }
}

void SearchEngineListModel::ChangeComboBoxSelection() {
  if (template_urls_.size()) {
    combo_box_->SetEnabled(true);

    const TemplateURL* default_search_provider =
        template_url_model_->GetDefaultSearchProvider();
    if (default_search_provider) {
      TemplateURLs::iterator i =
          find(template_urls_.begin(), template_urls_.end(),
               default_search_provider);
      if (i != template_urls_.end()) {
        combo_box_->SetSelectedItem(
            static_cast<int>(i - template_urls_.begin()));
      }
    }
  } else {
    combo_box_->SetEnabled(false);
  }
}

///////////////////////////////////////////////////////////////////////////////
// GeneralPageView, public:

GeneralPageView::GeneralPageView(Profile* profile)
    : startup_group_(NULL),
      startup_homepage_radio_(NULL),
      startup_last_session_radio_(NULL),
      startup_custom_radio_(NULL),
      startup_add_custom_page_button_(NULL),
      startup_remove_custom_page_button_(NULL),
      startup_use_current_page_button_(NULL),
      startup_custom_pages_table_(NULL),
      homepage_group_(NULL),
      homepage_use_newtab_radio_(NULL),
      homepage_use_url_radio_(NULL),
      homepage_use_url_textfield_(NULL),
      homepage_show_home_button_checkbox_(NULL),
      default_search_group_(NULL),
      default_search_manage_engines_button_(NULL),
      default_browser_group_(NULL),
      default_browser_status_label_(NULL),
      default_browser_use_as_default_button_(NULL),
      default_browser_worker_(new DefaultBrowserWorker(this)),
      OptionsPageView(profile) {
}

GeneralPageView::~GeneralPageView() {
  profile()->GetPrefs()->RemovePrefObserver(prefs::kRestoreOnStartup, this);
  profile()->GetPrefs()->RemovePrefObserver(
      prefs::kURLsToRestoreOnStartup, this);
  if (startup_custom_pages_table_)
    startup_custom_pages_table_->SetModel(NULL);
  default_browser_worker_->ViewDestroyed();
}

///////////////////////////////////////////////////////////////////////////////
// GeneralPageView, views::NativeButton::Listener implementation:

void GeneralPageView::ButtonPressed(views::NativeButton* sender) {
  if (sender == startup_homepage_radio_ ||
      sender == startup_last_session_radio_ ||
      sender == startup_custom_radio_) {
    SaveStartupPref();
    if (sender == startup_homepage_radio_) {
      UserMetricsRecordAction(L"Options_Startup_Homepage",
                              profile()->GetPrefs());
    } else if (sender == startup_last_session_radio_) {
      UserMetricsRecordAction(L"Options_Startup_LastSession",
                              profile()->GetPrefs());
    } else if (sender == startup_custom_radio_) {
      UserMetricsRecordAction(L"Options_Startup_Custom",
                              profile()->GetPrefs());
    }
  } else if (sender == startup_add_custom_page_button_) {
    AddURLToStartupURLs();
  } else if (sender == startup_remove_custom_page_button_) {
    RemoveURLsFromStartupURLs();
  } else if (sender == startup_use_current_page_button_) {
    SetStartupURLToCurrentPage();
  } else if (sender == homepage_use_newtab_radio_) {
    UserMetricsRecordAction(L"Options_Homepage_UseNewTab",
                            profile()->GetPrefs());
    SetHomepage(GetNewTabUIURLString());
    EnableHomepageURLField(false);
  } else if (sender == homepage_use_url_radio_) {
    UserMetricsRecordAction(L"Options_Homepage_UseURL",
                            profile()->GetPrefs());
    SetHomepage(homepage_use_url_textfield_->GetText());
    EnableHomepageURLField(true);
  } else if (sender == homepage_show_home_button_checkbox_) {
    bool show_button = homepage_show_home_button_checkbox_->IsSelected();
    if (show_button) {
      UserMetricsRecordAction(L"Options_Homepage_ShowHomeButton",
                              profile()->GetPrefs());
    } else {
      UserMetricsRecordAction(L"Options_Homepage_HideHomeButton",
                              profile()->GetPrefs());
    }
    show_home_button_.SetValue(show_button);
  } else if (sender == default_browser_use_as_default_button_) {
    default_browser_worker_->StartSetAsDefaultBrowser();
    UserMetricsRecordAction(L"Options_SetAsDefaultBrowser", NULL);
  } else if (sender == default_search_manage_engines_button_) {
    UserMetricsRecordAction(L"Options_ManageSearchEngines", NULL);
    KeywordEditorView::Show(profile());
  }
}

///////////////////////////////////////////////////////////////////////////////
// GeneralPageView, views::ComboBox::Listener implementation:

void GeneralPageView::ItemChanged(views::ComboBox* combo_box,
                                  int prev_index, int new_index) {
  if (combo_box == default_search_engine_combobox_) {
    SetDefaultSearchProvider();
    UserMetricsRecordAction(L"Options_SearchEngineChanged", NULL);
  }
}

///////////////////////////////////////////////////////////////////////////////
// GeneralPageView, views::TextField::Controller implementation:

void GeneralPageView::ContentsChanged(views::TextField* sender,
                                      const std::wstring& new_contents) {
  if (sender == homepage_use_url_textfield_) {
    // If the text field contains a valid URL, sync it to prefs. We run it
    // through the fixer upper to allow input like "google.com" to be converted
    // to something valid ("http://google.com").
    std::wstring url_string = URLFixerUpper::FixupURL(
        homepage_use_url_textfield_->GetText(), std::wstring());
    if (GURL(url_string).is_valid())
      SetHomepage(url_string);
  }
}

void GeneralPageView::HandleKeystroke(views::TextField* sender,
                                      UINT message, TCHAR key,
                                      UINT repeat_count, UINT flags) {
  // Not necessary.
}

///////////////////////////////////////////////////////////////////////////////
// GeneralPageView, OptionsPageView implementation:

void GeneralPageView::InitControlLayout() {
  using views::GridLayout;
  using views::ColumnSet;

  GridLayout* layout = new GridLayout(this);
  layout->SetInsets(5, 5, 5, 5);
  SetLayoutManager(layout);

  const int single_column_view_set_id = 0;
  ColumnSet* column_set = layout->AddColumnSet(single_column_view_set_id);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);
  layout->StartRow(0, single_column_view_set_id);
  InitStartupGroup();
  layout->AddView(startup_group_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  layout->StartRow(0, single_column_view_set_id);
  InitHomepageGroup();
  layout->AddView(homepage_group_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  layout->StartRow(0, single_column_view_set_id);
  InitDefaultSearchGroup();
  layout->AddView(default_search_group_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  layout->StartRow(0, single_column_view_set_id);
  InitDefaultBrowserGroup();
  layout->AddView(default_browser_group_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  // Register pref observers that update the controls when a pref changes.
  profile()->GetPrefs()->AddPrefObserver(prefs::kRestoreOnStartup, this);
  profile()->GetPrefs()->AddPrefObserver(prefs::kURLsToRestoreOnStartup, this);

  new_tab_page_is_home_page_.Init(prefs::kHomePageIsNewTabPage,
      profile()->GetPrefs(), this);
  homepage_.Init(prefs::kHomePage, profile()->GetPrefs(), this);
  show_home_button_.Init(prefs::kShowHomeButton, profile()->GetPrefs(), this);
}

void GeneralPageView::NotifyPrefChanged(const std::wstring* pref_name) {
  if (!pref_name || *pref_name == prefs::kRestoreOnStartup) {
    PrefService* prefs = profile()->GetPrefs();
    const SessionStartupPref startup_pref =
        SessionStartupPref::GetStartupPref(prefs);
    switch (startup_pref.type) {
    case SessionStartupPref::DEFAULT:
      startup_homepage_radio_->SetIsSelected(true);
      EnableCustomHomepagesControls(false);
      break;

    case SessionStartupPref::LAST:
      startup_last_session_radio_->SetIsSelected(true);
      EnableCustomHomepagesControls(false);
      break;

    case SessionStartupPref::URLS:
      startup_custom_radio_->SetIsSelected(true);
      EnableCustomHomepagesControls(true);
      break;
    }
  }

  // TODO(beng): Note that the kURLsToRestoreOnStartup pref is a mutable list,
  //             and changes to mutable lists aren't broadcast through the
  //             observer system, so the second half of this condition will
  //             never match. Once support for broadcasting such updates is
  //             added, this will automagically start to work, and this comment
  //             can be removed.
  if (!pref_name || *pref_name == prefs::kURLsToRestoreOnStartup) {
    PrefService* prefs = profile()->GetPrefs();
    const SessionStartupPref startup_pref =
        SessionStartupPref::GetStartupPref(prefs);
    startup_custom_pages_table_model_->SetURLs(startup_pref.urls);
  }

  if (!pref_name || *pref_name == prefs::kHomePageIsNewTabPage) {
    if (new_tab_page_is_home_page_.GetValue()) {
      homepage_use_newtab_radio_->SetIsSelected(true);
      EnableHomepageURLField(false);
    } else {
      homepage_use_url_radio_->SetIsSelected(true);
      EnableHomepageURLField(true);
    }
  }

  if (!pref_name || *pref_name == prefs::kHomePage) {
    bool enabled = homepage_.GetValue() != GetNewTabUIURLString();
    if (enabled)
      homepage_use_url_textfield_->SetText(homepage_.GetValue());
  }

  if (!pref_name || *pref_name == prefs::kShowHomeButton) {
    homepage_show_home_button_checkbox_->SetIsSelected(
        show_home_button_.GetValue());
  }
}

void GeneralPageView::HighlightGroup(OptionsGroup highlight_group) {
  if (highlight_group == OPTIONS_GROUP_DEFAULT_SEARCH)
    default_search_group_->SetHighlighted(true);
}

///////////////////////////////////////////////////////////////////////////////
// GeneralPageView, views::View overrides:

void GeneralPageView::Layout() {
  // We need to Layout twice - once to get the width of the contents box...
  View::Layout();
  startup_last_session_radio_->SetBounds(
      0, 0, startup_group_->GetContentsWidth(), 0);
  homepage_use_newtab_radio_->SetBounds(
      0, 0, homepage_group_->GetContentsWidth(), 0);
  homepage_show_home_button_checkbox_->SetBounds(
      0, 0, homepage_group_->GetContentsWidth(), 0);
  default_browser_status_label_->SetBounds(
      0, 0, default_browser_group_->GetContentsWidth(), 0);
  // ... and twice to get the height of multi-line items correct.
  View::Layout();
}

///////////////////////////////////////////////////////////////////////////////
// GeneralPageView, private:

void GeneralPageView::SetDefaultBrowserUIState(DefaultBrowserUIState state) {
  bool button_enabled = state == STATE_NOT_DEFAULT;
  default_browser_use_as_default_button_->SetEnabled(button_enabled);
  if (state == STATE_DEFAULT) {
    default_browser_status_label_->SetText(
      l10n_util::GetStringF(IDS_OPTIONS_DEFAULTBROWSER_DEFAULT,
                            l10n_util::GetString(IDS_PRODUCT_NAME)));
    default_browser_status_label_->SetColor(kDefaultBrowserLabelColor);
    Layout();
  } else if (state == STATE_NOT_DEFAULT) {
    default_browser_status_label_->SetText(
        l10n_util::GetStringF(IDS_OPTIONS_DEFAULTBROWSER_NOTDEFAULT,
                              l10n_util::GetString(IDS_PRODUCT_NAME)));
    default_browser_status_label_->SetColor(kNotDefaultBrowserLabelColor);
    Layout();
  }
}

void GeneralPageView::InitStartupGroup() {
  startup_homepage_radio_ = new views::RadioButton(
      l10n_util::GetString(IDS_OPTIONS_STARTUP_SHOW_DEFAULT_AND_NEWTAB),
      kStartupRadioGroup);
  startup_homepage_radio_->SetListener(this);
  startup_last_session_radio_ = new views::RadioButton(
      l10n_util::GetString(IDS_OPTIONS_STARTUP_SHOW_LAST_SESSION),
      kStartupRadioGroup);
  startup_last_session_radio_->SetListener(this);
  startup_last_session_radio_->SetMultiLine(true);
  startup_custom_radio_ = new views::RadioButton(
      l10n_util::GetString(IDS_OPTIONS_STARTUP_SHOW_PAGES),
      kStartupRadioGroup);
  startup_custom_radio_->SetListener(this);
  startup_add_custom_page_button_ = new views::NativeButton(
      l10n_util::GetString(IDS_OPTIONS_STARTUP_ADD_BUTTON));
  startup_add_custom_page_button_->SetListener(this);
  startup_remove_custom_page_button_ = new views::NativeButton(
      l10n_util::GetString(IDS_OPTIONS_STARTUP_REMOVE_BUTTON));
  startup_remove_custom_page_button_->SetEnabled(false);
  startup_remove_custom_page_button_->SetListener(this);
  startup_use_current_page_button_ = new views::NativeButton(
      l10n_util::GetString(IDS_OPTIONS_STARTUP_USE_CURRENT));
  startup_use_current_page_button_->SetListener(this);

  startup_custom_pages_table_model_.reset(
      new CustomHomePagesTableModel(profile()));
  std::vector<views::TableColumn> columns;
  columns.push_back(views::TableColumn());
  startup_custom_pages_table_ = new views::TableView(
      startup_custom_pages_table_model_.get(), columns,
      views::ICON_AND_TEXT, true, false, true);
  // URLs are inherently left-to-right, so do not mirror the table.
  startup_custom_pages_table_->EnableUIMirroringForRTLLanguages(false);
  startup_custom_pages_table_->SetObserver(this);

  using views::GridLayout;
  using views::ColumnSet;

  views::View* contents = new views::View;
  GridLayout* layout = new GridLayout(contents);
  contents->SetLayoutManager(layout);

  const int single_column_view_set_id = 0;
  ColumnSet* column_set = layout->AddColumnSet(single_column_view_set_id);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 1,
                        GridLayout::USE_PREF, 0, 0);

  const int double_column_view_set_id = 1;
  column_set = layout->AddColumnSet(double_column_view_set_id);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 0,
                        GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(startup_homepage_radio_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(startup_last_session_radio_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(startup_custom_radio_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  layout->StartRow(0, double_column_view_set_id);
  layout->AddView(startup_custom_pages_table_);

  views::View* button_stack = new views::View;
  GridLayout* button_stack_layout = new GridLayout(button_stack);
  button_stack->SetLayoutManager(button_stack_layout);

  column_set = button_stack_layout->AddColumnSet(single_column_view_set_id);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 1,
                        GridLayout::USE_PREF, 0, 0);
  button_stack_layout->StartRow(0, single_column_view_set_id);
  button_stack_layout->AddView(startup_add_custom_page_button_,
                               1, 1, GridLayout::FILL, GridLayout::CENTER);
  button_stack_layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  button_stack_layout->StartRow(0, single_column_view_set_id);
  button_stack_layout->AddView(startup_remove_custom_page_button_,
                               1, 1, GridLayout::FILL, GridLayout::CENTER);
  button_stack_layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  button_stack_layout->StartRow(0, single_column_view_set_id);
  button_stack_layout->AddView(startup_use_current_page_button_,
                               1, 1, GridLayout::FILL, GridLayout::CENTER);
  layout->AddView(button_stack);

  startup_group_ = new OptionsGroupView(
      contents, l10n_util::GetString(IDS_OPTIONS_STARTUP_GROUP_NAME),
      EmptyWString(), true);
}

void GeneralPageView::InitHomepageGroup() {
  homepage_use_newtab_radio_ = new views::RadioButton(
      l10n_util::GetString(IDS_OPTIONS_HOMEPAGE_USE_NEWTAB),
      kHomePageRadioGroup);
  homepage_use_newtab_radio_->SetListener(this);
  homepage_use_newtab_radio_->SetMultiLine(true);
  homepage_use_url_radio_ = new views::RadioButton(
      l10n_util::GetString(IDS_OPTIONS_HOMEPAGE_USE_URL),
      kHomePageRadioGroup);
  homepage_use_url_radio_->SetListener(this);
  homepage_use_url_textfield_ = new views::TextField;
  homepage_use_url_textfield_->SetController(this);
  homepage_show_home_button_checkbox_ = new views::CheckBox(
      l10n_util::GetString(IDS_OPTIONS_HOMEPAGE_SHOW_BUTTON));
  homepage_show_home_button_checkbox_->SetListener(this);
  homepage_show_home_button_checkbox_->SetMultiLine(true);

  using views::GridLayout;
  using views::ColumnSet;

  views::View* contents = new views::View;
  GridLayout* layout = new GridLayout(contents);
  contents->SetLayoutManager(layout);

  const int single_column_view_set_id = 0;
  ColumnSet* column_set = layout->AddColumnSet(single_column_view_set_id);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 1,
                        GridLayout::USE_PREF, 0, 0);

  const int double_column_view_set_id = 1;
  column_set = layout->AddColumnSet(double_column_view_set_id);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(homepage_use_newtab_radio_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, double_column_view_set_id);
  layout->AddView(homepage_use_url_radio_);
  layout->AddView(homepage_use_url_textfield_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(homepage_show_home_button_checkbox_);

  homepage_group_ = new OptionsGroupView(
      contents, l10n_util::GetString(IDS_OPTIONS_HOMEPAGE_GROUP_NAME),
      EmptyWString(), true);
}


void GeneralPageView::InitDefaultSearchGroup() {
  default_search_engines_model_.reset(new SearchEngineListModel(profile()));
  default_search_engine_combobox_ =
      new views::ComboBox(default_search_engines_model_.get());
  default_search_engines_model_->SetComboBox(default_search_engine_combobox_);
  default_search_engine_combobox_->SetListener(this);

  default_search_manage_engines_button_ = new views::NativeButton(
      l10n_util::GetString(IDS_OPTIONS_DEFAULTSEARCH_MANAGE_ENGINES_LINK));
  default_search_manage_engines_button_->SetListener(this);

  using views::GridLayout;
  using views::ColumnSet;

  views::View* contents = new views::View;
  GridLayout* layout = new GridLayout(contents);
  contents->SetLayoutManager(layout);

  const int double_column_view_set_id = 0;
  ColumnSet* column_set = layout->AddColumnSet(double_column_view_set_id);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, double_column_view_set_id);
  layout->AddView(default_search_engine_combobox_);
  layout->AddView(default_search_manage_engines_button_);

  default_search_group_ = new OptionsGroupView(
      contents, l10n_util::GetString(IDS_OPTIONS_DEFAULTSEARCH_GROUP_NAME),
      EmptyWString(), true);
}

void GeneralPageView::InitDefaultBrowserGroup() {
  default_browser_status_label_ = new views::Label;
  default_browser_status_label_->SetMultiLine(true);
  default_browser_status_label_->SetHorizontalAlignment(
      views::Label::ALIGN_LEFT);
  default_browser_use_as_default_button_ = new views::NativeButton(
      l10n_util::GetStringF(IDS_OPTIONS_DEFAULTBROWSER_USEASDEFAULT,
                            l10n_util::GetString(IDS_PRODUCT_NAME)));
  default_browser_use_as_default_button_->SetListener(this);

  using views::GridLayout;
  using views::ColumnSet;

  views::View* contents = new views::View;
  GridLayout* layout = new GridLayout(contents);
  contents->SetLayoutManager(layout);

  const int single_column_view_set_id = 0;
  ColumnSet* column_set = layout->AddColumnSet(single_column_view_set_id);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 1,
                        GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(default_browser_status_label_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(default_browser_use_as_default_button_);

  default_browser_group_ = new OptionsGroupView(
      contents, l10n_util::GetString(IDS_OPTIONS_DEFAULTBROWSER_GROUP_NAME),
      EmptyWString(), false);

  default_browser_worker_->StartCheckDefaultBrowser();
}

void GeneralPageView::SaveStartupPref() {
  SessionStartupPref pref;

  if (startup_last_session_radio_->IsSelected()) {
    pref.type = SessionStartupPref::LAST;
  } else if (startup_custom_radio_->IsSelected()) {
    pref.type = SessionStartupPref::URLS;
  }

  pref.urls = startup_custom_pages_table_model_->GetURLs();

  SessionStartupPref::SetStartupPref(profile()->GetPrefs(), pref);
}

void GeneralPageView::AddURLToStartupURLs() {
  ShelfItemDialog* dialog = new ShelfItemDialog(this, profile(), false);
  dialog->Show(GetRootWindow());
}

void GeneralPageView::RemoveURLsFromStartupURLs() {
  for (views::TableView::iterator i =
       startup_custom_pages_table_->SelectionBegin();
       i != startup_custom_pages_table_->SelectionEnd(); ++i) {
    startup_custom_pages_table_model_->Remove(*i);
  }
  SaveStartupPref();
}

void GeneralPageView::SetStartupURLToCurrentPage() {
  // Remove the current entries.
  while (startup_custom_pages_table_model_->RowCount())
    startup_custom_pages_table_model_->Remove(0);

  // And add all entries for all open browsers with our profile.
  int add_index = 0;
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
          startup_custom_pages_table_model_->Add(add_index++, url);
      }
    }
  }

  SaveStartupPref();
}

void GeneralPageView::EnableCustomHomepagesControls(bool enable) {
  startup_add_custom_page_button_->SetEnabled(enable);
  bool has_selected_rows = startup_custom_pages_table_->SelectedRowCount() > 0;
  startup_remove_custom_page_button_->SetEnabled(enable && has_selected_rows);
  startup_use_current_page_button_->SetEnabled(enable);
  startup_custom_pages_table_->SetEnabled(enable);
}

void GeneralPageView::AddBookmark(ShelfItemDialog* dialog,
                                  const std::wstring& title,
                                  const GURL& url) {
  int index = startup_custom_pages_table_->FirstSelectedRow();
  if (index == -1)
    index = startup_custom_pages_table_model_->RowCount();
  else
    index++;
  startup_custom_pages_table_model_->Add(index, url);

  SaveStartupPref();
}

void GeneralPageView::SetHomepage(const std::wstring& homepage) {
  if (homepage.empty() || homepage == GetNewTabUIURLString()) {
    new_tab_page_is_home_page_.SetValue(true);
  } else {
    new_tab_page_is_home_page_.SetValue(false);
    homepage_.SetValue(homepage);
  }
}

void GeneralPageView::OnSelectionChanged() {
  startup_remove_custom_page_button_->SetEnabled(
      startup_custom_pages_table_->SelectedRowCount() > 0);
}

void GeneralPageView::EnableHomepageURLField(bool enabled) {
  if (enabled) {
    homepage_use_url_textfield_->SetEnabled(true);
    homepage_use_url_textfield_->SetReadOnly(false);
  } else {
    homepage_use_url_textfield_->SetEnabled(false);
    homepage_use_url_textfield_->SetReadOnly(true);
  }
}

void GeneralPageView::SetDefaultSearchProvider() {
  const int index = default_search_engine_combobox_->GetSelectedItem();
  default_search_engines_model_->model()->SetDefaultSearchProvider(
      default_search_engines_model_->GetTemplateURLAt(index));
}

