// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <windows.h>
#include <shlobj.h>
#include <vsstyle.h>
#include <vssym32.h>

#include "chrome/browser/views/options/languages_page_view.h"

#include "base/file_util.h"
#include "base/string_util.h"
#include "base/gfx/native_theme.h"
#include "base/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/shell_dialogs.h"
#include "chrome/browser/spellchecker.h"
#include "chrome/browser/views/options/language_combobox_model.h"
#include "chrome/browser/views/password_manager_view.h"
#include "chrome/browser/views/restart_message_box.h"
#include "chrome/browser/views/standard_layout.h"
#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/common/gfx/chrome_font.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/views/controls/button/radio_button.h"
#include "chrome/views/controls/combo_box.h"
#include "chrome/views/controls/tabbed_pane.h"
#include "chrome/views/controls/text_field.h"
#include "chrome/views/grid_layout.h"
#include "chrome/views/widget/widget.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "skia/include/SkBitmap.h"
#include "unicode/uloc.h"

static const char* const accept_language_list[] = {
  "af",     // Afrikaans
  "am",     // Amharic
  "ar",     // Arabic
  "az",     // Azerbaijani
  "be",     // Belarusian
  "bg",     // Bulgarian
  "bh",     // Bihari
  "bn",     // Bengali
  "br",     // Breton
  "bs",     // Bosnian
  "ca",     // Catalan
  "co",     // Corsican
  "cs",     // Czech
  "cy",     // Welsh
  "da",     // Danish
  "de",     // German
  "de-AT",  // German (Austria)
  "de-CH",  // German (Switzerland)
  "de-DE",  // German (Germany)
  "el",     // Greek
  "en",     // English
  "en-AU",  // English (Austrailia)
  "en-CA",  // English (Canada)
  "en-GB",  // English (UK)
  "en-NZ",  // English (New Zealand)
  "en-US",  // English (US)
  "en-ZA",  // English (South Africa)
  "eo",     // Esperanto
  // TODO(jungshik) : Do we want to list all es-Foo for Latin-American
  // Spanish speaking countries?
  "es",     // Spanish
  "et",     // Estonian
  "eu",     // Basque
  "fa",     // Persian
  "fi",     // Finnish
  "fil",    // Filipino
  "fo",     // Faroese
  "fr",     // French
  "fr-CA",  // French (Canada)
  "fr-CH",  // French (Switzerland)
  "fr-FR",  // French (France)
  "fy",     // Frisian
  "ga",     // Irish
  "gd",     // Scots Gaelic
  "gl",     // Galician
  "gn",     // Guarani
  "gu",     // Gujarati
  "he",     // Hebrew
  "hi",     // Hindi
  "hr",     // Croatian
  "hu",     // Hungarian
  "hy",     // Armenian
  "ia",     // Interlingua
  "id",     // Indonesian
  "is",     // Icelandic
  "it",     // Italian
  "it-CH",  // Italian (Switzerland)
  "it-IT",  // Italian (Italy)
  "ja",     // Japanese
  "jw",     // Javanese
  "ka",     // Georgian
  "kk",     // Kazakh
  "km",     // Cambodian
  "kn",     // Kannada
  "ko",     // Korean
  "ku",     // Kurdish
  "ky",     // Kyrgyz
  "la",     // Latin
  "ln",     // Lingala
  "lo",     // Laothian
  "lt",     // Lithuanian
  "lv",     // Latvian
  "mk",     // Macedonian
  "ml",     // Malayalam
  "mn",     // Mongolian
  "mo",     // Moldavian
  "mr",     // Marathi
  "ms",     // Malay
  "mt",     // Maltese
  "nb",     // Norwegian (Bokmal)
  "ne",     // Nepali
  "nl",     // Dutch
  "nn",     // Norwegian (Nynorsk)
  "no",     // Norwegian
  "oc",     // Occitan
  "or",     // Oriya
  "pa",     // Punjabi
  "pl",     // Polish
  "ps",     // Pashto
  "pt",     // Portuguese
  "pt-BR",  // Portuguese (Brazil)
  "pt-PT",  // Portuguese (Portugal)
  "qu",     // Quechua
  "rm",     // Romansh
  "ro",     // Romanian
  "ru",     // Russian
  "sd",     // Sindhi
  "sh",     // Serbo-Croatian
  "si",     // Sinhalese
  "sk",     // Slovak
  "sl",     // Slovenian
  "sn",     // Shona
  "so",     // Somali
  "sq",     // Albanian
  "sr",     // Serbian
  "st",     // Sesotho
  "su",     // Sundanese
  "sv",     // Swedish
  "sw",     // Swahili
  "ta",     // Tamil
  "te",     // Telugu
  "tg",     // Tajik
  "th",     // Thai
  "ti",     // Tigrinya
  "tk",     // Turkmen
  "to",     // Tonga
  "tr",     // Turkish
  "tt",     // Tatar
  "tw",     // Twi
  "ug",     // Uighur
  "uk",     // Ukrainian
  "ur",     // Urdu
  "uz",     // Uzbek
  "vi",     // Vietnamese
  "xh",     // Xhosa
  "yi",     // Yiddish
  "yo",     // Yoruba
  "zh",     // Chinese
  "zh-CN",  // Chinese (Simplified)
  "zh-TW",  // Chinese (Traditional)
  "zu",     // Zulu
};

///////////////////////////////////////////////////////////////////////////////
// AddLanguageWindowView
//
// This opens another window from where a new accept language can be selected.
//
class AddLanguageWindowView : public views::View,
                              public views::ComboBox::Listener,
                              public views::DialogDelegate {
 public:
  AddLanguageWindowView(LanguagesPageView* language_delegate, Profile* profile);
  views::Window* container() const { return container_; }
  void set_container(views::Window* container) {
    container_ = container;
  }

  // views::DialogDelegate methods.
  virtual bool Accept();
  virtual std::wstring GetWindowTitle() const;

  // views::WindowDelegate method.
  virtual bool IsModal() const { return true; }
  virtual views::View* GetContentsView() { return this; }

  // views::ComboBox::Listener implementation:
  virtual void ItemChanged(views::ComboBox* combo_box,
                           int prev_index,
                           int new_index);

  // views::View overrides.
  virtual void Layout();
  virtual gfx::Size GetPreferredSize();

 protected:
  virtual void ViewHierarchyChanged(bool is_add, views::View* parent,
                                    views::View* child);

 private:
  void Init();

  // The Options dialog window.
  views::Window* container_;

  // Used for Call back to LanguagePageView that language has been selected.
  LanguagesPageView* language_delegate_;
  std::string accept_language_selected_;

  // Combobox and its corresponding model.
  scoped_ptr<LanguageComboboxModel> accept_language_combobox_model_;
  views::ComboBox* accept_language_combobox_;

  // The Profile associated with this window.
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(AddLanguageWindowView);
};

static const int kDialogPadding = 7;
static int kDefaultWindowWidthChars = 60;
static int kDefaultWindowHeightLines = 3;

AddLanguageWindowView::AddLanguageWindowView(
    LanguagesPageView* language_delegate,
    Profile* profile)
    : profile_(profile->GetOriginalProfile()),
      language_delegate_(language_delegate),
      accept_language_combobox_(NULL) {
  Init();

  // Initialize accept_language_selected_ to the first index in drop down.
  accept_language_selected_ = accept_language_combobox_model_->
      GetLocaleFromIndex(0);
}

std::wstring AddLanguageWindowView::GetWindowTitle() const {
  return l10n_util::GetString(IDS_FONT_LANGUAGE_SETTING_LANGUAGES_TAB_TITLE);
}

bool AddLanguageWindowView::Accept() {
  if (language_delegate_) {
    language_delegate_->OnAddLanguage(accept_language_selected_);
  }
  return true;
}

void AddLanguageWindowView::ItemChanged(views::ComboBox* combo_box,
                                  int prev_index,
                                  int new_index) {
  accept_language_selected_ = accept_language_combobox_model_->
      GetLocaleFromIndex(new_index);
}

void AddLanguageWindowView::Layout() {
  gfx::Size sz = accept_language_combobox_->GetPreferredSize();
  accept_language_combobox_->SetBounds(kDialogPadding, kDialogPadding,
                                       width() - 2*kDialogPadding,
                                       sz.height());
}

gfx::Size AddLanguageWindowView::GetPreferredSize() {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  ChromeFont font = rb.GetFont(ResourceBundle::BaseFont);
  return gfx::Size(font.ave_char_width() * kDefaultWindowWidthChars,
                   font.height() * kDefaultWindowHeightLines);
}

void AddLanguageWindowView::ViewHierarchyChanged(bool is_add,
                                                 views::View* parent,
                                                 views::View* child) {
  // Can't init before we're inserted into a Widget, because we require
  // a HWND to parent native child controls to.
  if (is_add && child == this)
    Init();
}

void AddLanguageWindowView::Init() {
  // Determine Locale Codes.
  std::vector<std::string> locale_codes;
  const std::wstring app_locale = g_browser_process->GetApplicationLocale();
  for (size_t i = 0; i < arraysize(accept_language_list); ++i) {
    std::wstring local_name =
        l10n_util::GetLocalName(accept_language_list[i], app_locale, false);
    // This is a hack. If ICU doesn't have a translated name for
    // this language, GetLocalName will just return the language code.
    // In that case, we skip it.
    // TODO(jungshik) : Put them at the of the list with language codes
    // enclosed by brackets.
    if (IsStringASCII(local_name) &&
        WideToASCII(local_name) == accept_language_list[i])
      continue;
    locale_codes.push_back(accept_language_list[i]);
  }
  accept_language_combobox_model_.reset(new LanguageComboboxModel(
    profile_, locale_codes));
  accept_language_combobox_ = new views::ComboBox(
      accept_language_combobox_model_.get());
  accept_language_combobox_->SetSelectedItem(0);
  accept_language_combobox_->SetListener(this);
  AddChildView(accept_language_combobox_);
}

class LanguageOrderTableModel : public views::TableModel {
 public:
  LanguageOrderTableModel();

  // Set Language List.
  void SetAcceptLanguagesString(const std::string& language_list);

  // Add at the end.
  void Add(const std::string& language);

  // Removes the entry at the specified index.
  void Remove(int index);

  // Returns index corresponding to a given language. Returns -1 if the
  // language is not found.
  int GetIndex(const std::string& language);

  // Move down the entry at the specified index.
  void MoveDown(int index);

  // Move up the entry at the specified index.
  void MoveUp(int index);

  // Returns the set of languagess this model contains.
  std::string GetLanguageList() { return VectorToList(languages_); }

  // views::TableModel overrides:
  virtual int RowCount();
  virtual std::wstring GetText(int row, int column_id);
  virtual void SetObserver(views::TableModelObserver* observer);

 private:
  // This method converts a comma separated list to a vector of strings.
  void ListToVector(const std::string& list,
                    std::vector<std::string>* vector);

  // This method returns a comma separated string given a string vector.
  std::string VectorToList(const std::vector<std::string>& vector);

  // Set of entries we're showing.
  std::vector<std::string> languages_;
  std::string comma_separated_language_list_;

  views::TableModelObserver* observer_;

  DISALLOW_COPY_AND_ASSIGN(LanguageOrderTableModel);
};

LanguageOrderTableModel::LanguageOrderTableModel()
    : observer_(NULL) {
}

void LanguageOrderTableModel::SetAcceptLanguagesString(
    const std::string& language_list) {
  std::vector<std::string> languages_vector;
  ListToVector(language_list, &languages_vector);
  for (int i = 0; i < static_cast<int>(languages_vector.size()); i++) {
    Add(languages_vector.at(i));
  }
}

void LanguageOrderTableModel::SetObserver(
    views::TableModelObserver* observer) {
  observer_ = observer;
}

std::wstring LanguageOrderTableModel::GetText(int row, int column_id) {
  DCHECK(row >= 0 && row < RowCount());
  const std::wstring app_locale = g_browser_process->GetApplicationLocale();
  return l10n_util::GetLocalName(languages_.at(row), app_locale, true);
}

void LanguageOrderTableModel::Add(const std::string& language) {
  if (language.empty())
    return;
  // Check for selecting duplicated language.
  for (std::vector<std::string>::const_iterator cit = languages_.begin();
       cit != languages_.end(); ++cit)
    if (*cit == language)
      return;
  languages_.push_back(language);
  if (observer_)
    observer_->OnItemsAdded(RowCount() - 1, 1);
}

void LanguageOrderTableModel::Remove(int index) {
  DCHECK(index >= 0 && index < RowCount());
  languages_.erase(languages_.begin() + index);
  if (observer_)
    observer_->OnItemsRemoved(index, 1);
}

int LanguageOrderTableModel::GetIndex(const std::string& language) {
  if (language.empty())
    return -1;

  int index = 0;
  for (std::vector<std::string>::const_iterator cit = languages_.begin();
      cit != languages_.end(); ++cit) {
    if (*cit == language)
      return index;

    index++;
  }

  return -1;
}

void LanguageOrderTableModel::MoveDown(int index) {
  if (index < 0 || index >= RowCount() - 1)
    return;
  std::string item = languages_.at(index);
  languages_.erase(languages_.begin() + index);
  if (index == RowCount() - 1)
    languages_.push_back(item);
  else
    languages_.insert(languages_.begin() + index + 1, item);
  if (observer_)
    observer_->OnItemsChanged(0, RowCount());
}

void LanguageOrderTableModel::MoveUp(int index) {
  if (index <= 0 || index >= static_cast<int>(languages_.size()))
    return;
  std::string item = languages_.at(index);
  languages_.erase(languages_.begin() + index);
  languages_.insert(languages_.begin() + index - 1, item);
  if (observer_)
    observer_->OnItemsChanged(0, RowCount());
}

int LanguageOrderTableModel::RowCount() {
  return static_cast<int>(languages_.size());
}

void LanguageOrderTableModel::ListToVector(const std::string& list,
                                           std::vector<std::string>* vector) {
  SplitString(list, ',', vector);
}

std::string LanguageOrderTableModel::VectorToList(
    const std::vector<std::string>& vector)  {
  std::string list;
  for (int i = 0 ; i < static_cast<int>(vector.size()) ; i++) {
    list += vector.at(i);
    if (i != vector.size() - 1)
      list += ',';
  }
  return list;
}

LanguagesPageView::LanguagesPageView(Profile* profile)
    : languages_instructions_(NULL),
      languages_contents_(NULL),
      language_order_table_(NULL),
      add_button_(NULL),
      remove_button_(NULL),
      move_up_button_(NULL),
      move_down_button_(NULL),
      button_stack_(NULL),
      language_info_label_(NULL),
      ui_language_label_(NULL),
      change_ui_language_combobox_(NULL),
      change_dictionary_language_combobox_(NULL),
      enable_spellchecking_checkbox_(NULL),
      dictionary_language_label_(NULL),
      OptionsPageView(profile),
      language_table_edited_(false),
      language_warning_shown_(false),
      spellcheck_language_index_selected_(-1),
      ui_language_index_selected_(-1),
      starting_ui_language_index_(-1) {
  accept_languages_.Init(prefs::kAcceptLanguages,
      profile->GetPrefs(), NULL);
  enable_spellcheck_.Init(prefs::kEnableSpellCheck,
      profile->GetPrefs(), NULL);
}

LanguagesPageView::~LanguagesPageView() {
  if (language_order_table_)
    language_order_table_->SetModel(NULL);
}

void LanguagesPageView::ButtonPressed(views::Button* sender) {
  if (sender == move_up_button_) {
    OnMoveUpLanguage();
    language_table_edited_ = true;
  } else if (sender == move_down_button_) {
    OnMoveDownLanguage();
    language_table_edited_ = true;
  } else if (sender == remove_button_) {
    OnRemoveLanguage();
    language_table_edited_ = true;
  } else if (sender == add_button_) {
    views::Window::CreateChromeWindow(
        GetWidget()->GetNativeView(),
        gfx::Rect(),
        new AddLanguageWindowView(this, profile()))->Show();
    language_table_edited_ = true;
  } else if (sender == enable_spellchecking_checkbox_) {
    if (enable_spellchecking_checkbox_->checked())
      enable_spellcheck_.SetValue(true);
    else
      enable_spellcheck_.SetValue(false);
  }
}

void LanguagesPageView::OnAddLanguage(const std::string& new_language) {
  language_order_table_model_->Add(new_language);
  language_order_table_->Select(language_order_table_model_->RowCount() - 1);
  OnSelectionChanged();
}

void LanguagesPageView::InitControlLayout() {
  // Define the buttons.
  add_button_ = new views::NativeButton(this, l10n_util::GetString(
      IDS_FONT_LANGUAGE_SETTING_LANGUAGES_SELECTOR_ADD_BUTTON_LABEL));
  remove_button_ = new views::NativeButton(this, l10n_util::GetString(
      IDS_FONT_LANGUAGE_SETTING_LANGUAGES_SELECTOR_REMOVE_BUTTON_LABEL));
  remove_button_->SetEnabled(false);
  move_up_button_ = new views::NativeButton(this, l10n_util::GetString(
      IDS_FONT_LANGUAGE_SETTING_LANGUAGES_SELECTOR_MOVEUP_BUTTON_LABEL));
  move_up_button_->SetEnabled(false);
  move_down_button_ = new views::NativeButton(this, l10n_util::GetString(
      IDS_FONT_LANGUAGE_SETTING_LANGUAGES_SELECTOR_MOVEDOWN_BUTTON_LABEL));
  move_down_button_->SetEnabled(false);

  languages_contents_ = new views::View;
  using views::GridLayout;
  using views::ColumnSet;

  GridLayout* layout = CreatePanelGridLayout(this);
  SetLayoutManager(layout);

  const int single_column_view_set_id = 0;
  ColumnSet* column_set = layout->AddColumnSet(single_column_view_set_id);

  // Add the instructions label.
  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 1,
                      GridLayout::USE_PREF, 0, 0);
  languages_instructions_ = new views::Label(
      l10n_util::GetString(
          IDS_FONT_LANGUAGE_SETTING_LANGUAGES_INSTRUCTIONS));
  languages_instructions_->SetMultiLine(true);
  languages_instructions_->SetHorizontalAlignment(
      views::Label::ALIGN_LEFT);
  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(languages_instructions_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  // Add two columns - for table, and for button stack.
  std::vector<views::TableColumn> columns;
  columns.push_back(views::TableColumn());
  language_order_table_model_.reset(new LanguageOrderTableModel);
  language_order_table_ = new views::TableView(
      language_order_table_model_.get(), columns,
      views::TEXT_ONLY, false, true, true);
  language_order_table_->SetObserver(this);

  const int double_column_view_set_id = 1;
  column_set = layout->AddColumnSet(double_column_view_set_id);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 0,
                        GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, double_column_view_set_id);

  // Add the table to the the first column.
  layout->AddView(language_order_table_);

  // Now add the four buttons to the second column.
  button_stack_ = new views::View;
  GridLayout* button_stack_layout = new GridLayout(button_stack_);
  button_stack_->SetLayoutManager(button_stack_layout);

  column_set = button_stack_layout->AddColumnSet(single_column_view_set_id);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 1,
                        GridLayout::USE_PREF, 0, 0);
  button_stack_layout->StartRow(0, single_column_view_set_id);
  button_stack_layout->AddView(move_up_button_, 1, 1, GridLayout::FILL,
                               GridLayout::CENTER);
  button_stack_layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  button_stack_layout->StartRow(0, single_column_view_set_id);
  button_stack_layout->AddView(move_down_button_, 1, 1, GridLayout::FILL,
                               GridLayout::CENTER);
  button_stack_layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  button_stack_layout->StartRow(0, single_column_view_set_id);
  button_stack_layout->AddView(remove_button_, 1, 1, GridLayout::FILL,
                               GridLayout::CENTER);
  button_stack_layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  button_stack_layout->StartRow(0, single_column_view_set_id);
  button_stack_layout->AddView(add_button_, 1, 1, GridLayout::FILL,
                               GridLayout::CENTER);

  layout->AddView(button_stack_);

  layout->AddPaddingRow(0, kUnrelatedControlVerticalSpacing);

  language_info_label_ = new views::Label(
      l10n_util::GetString(IDS_OPTIONS_CHROME_LANGUAGE_INFO));
  language_info_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  ui_language_label_ = new views::Label(
      l10n_util::GetString(IDS_OPTIONS_CHROME_UI_LANGUAGE));
  ui_language_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  ui_language_model_.reset(new LanguageComboboxModel);
  change_ui_language_combobox_ =
      new views::ComboBox(ui_language_model_.get());
  change_ui_language_combobox_->SetListener(this);
  dictionary_language_label_ = new views::Label(
      l10n_util::GetString(IDS_OPTIONS_CHROME_DICTIONARY_LANGUAGE));
  dictionary_language_label_->SetHorizontalAlignment(
      views::Label::ALIGN_LEFT);
  enable_spellchecking_checkbox_ = new views::Checkbox(
      l10n_util::GetString(IDS_OPTIONS_ENABLE_SPELLCHECK));
  enable_spellchecking_checkbox_->set_listener(this);
  enable_spellchecking_checkbox_->SetMultiLine(true);

  // Determine Locale Codes.
  SpellChecker::Languages spell_check_languages;
  SpellChecker::SpellCheckLanguages(&spell_check_languages);
  dictionary_language_model_.reset(new LanguageComboboxModel(profile(),
      spell_check_languages));
  change_dictionary_language_combobox_ =
      new views::ComboBox(dictionary_language_model_.get());
  change_dictionary_language_combobox_->SetListener(this);

  // SpellCheck language settings.
  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(enable_spellchecking_checkbox_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  const int double_column_view_set_2_id = 2;
  column_set = layout->AddColumnSet(double_column_view_set_2_id);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, double_column_view_set_2_id);
  layout->AddView(dictionary_language_label_);
  layout->AddView(change_dictionary_language_combobox_);

  // UI language settings.
  layout->AddPaddingRow(0, kUnrelatedControlVerticalSpacing);
  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(language_info_label_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  layout->StartRow(0, double_column_view_set_2_id);
  layout->AddView(ui_language_label_);
  layout->AddView(change_ui_language_combobox_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  // Init member prefs so we can update the controls if prefs change.
  app_locale_.Init(prefs::kApplicationLocale,
                   g_browser_process->local_state(), this);
  dictionary_language_.Init(prefs::kSpellCheckDictionary,
                            profile()->GetPrefs(), this);
}

void LanguagesPageView::NotifyPrefChanged(const std::wstring* pref_name) {
  if (!pref_name || *pref_name == prefs::kAcceptLanguages) {
    language_order_table_model_->SetAcceptLanguagesString(
        WideToASCII(accept_languages_.GetValue()));
  }
  if (!pref_name || *pref_name == prefs::kApplicationLocale) {
    int index = ui_language_model_->GetSelectedLanguageIndex(
        prefs::kApplicationLocale);
    if (-1 == index) {
      // The pref value for locale isn't valid.  Use the current app locale
      // (which is what we're currently using).
      index = ui_language_model_->GetIndexFromLocale(
          WideToASCII(g_browser_process->GetApplicationLocale()));
    }
    DCHECK(-1 != index);
    change_ui_language_combobox_->SetSelectedItem(index);
    starting_ui_language_index_ = index;
  }
  if (!pref_name || *pref_name == prefs::kSpellCheckDictionary) {
    int index = dictionary_language_model_->GetSelectedLanguageIndex(
        prefs::kSpellCheckDictionary);

    // If the index for the current language cannot be found, it is due to
    // the fact that the pref-member value for the last dictionary language
    // set by the user still uses the old format; i.e. language-region, even
    // when region is not necessary. For example, if the user sets the
    // dictionary language to be French, the pref-member value in the user
    // profile is "fr-FR", whereas we now use only "fr". To resolve this issue,
    // if "fr-FR" is read from the pref, the language code ("fr" here) is
    // extracted, and re-written in the pref, so that the pref-member value for
    // dictionary language in the user profile now correctly stores "fr"
    // instead of "fr-FR".
    if (index < 0) {
      PrefService* local_state;
      if (!profile())
        local_state = g_browser_process->local_state();
      else
        local_state = profile()->GetPrefs();

      DCHECK(local_state);
      const std::string& lang_region = WideToASCII(
          local_state->GetString(prefs::kSpellCheckDictionary));
      dictionary_language_.SetValue(ASCIIToWide(
          SpellChecker::GetLanguageFromLanguageRegion(lang_region)));
      index = dictionary_language_model_->GetSelectedLanguageIndex(
          prefs::kSpellCheckDictionary);
    }

    change_dictionary_language_combobox_->SetSelectedItem(index);
    spellcheck_language_index_selected_ = -1;
  }
  if (!pref_name || *pref_name == prefs::kEnableSpellCheck) {
    enable_spellchecking_checkbox_->SetChecked(
        enable_spellcheck_.GetValue());
  }
}

void LanguagesPageView::ItemChanged(views::ComboBox* sender,
                                    int prev_index,
                                    int new_index) {
  if (prev_index == new_index)
    return;

  if (sender == change_ui_language_combobox_) {
    if (new_index == starting_ui_language_index_)
      ui_language_index_selected_ = -1;
    else
      ui_language_index_selected_ = new_index;

    if (!language_warning_shown_) {
      RestartMessageBox::ShowMessageBox(GetRootWindow());
      language_warning_shown_ = true;
    }
  } else if (sender == change_dictionary_language_combobox_) {
    // Set the spellcheck language selected.
    spellcheck_language_index_selected_ = new_index;

    // Remove the previously added spell check language to the accept list.
    if (!spellcheck_language_added_.empty()) {
      int old_index = language_order_table_model_->GetIndex(
          spellcheck_language_added_);
      if (old_index > -1)
        language_order_table_model_->Remove(old_index);
    }

    // Add this new spell check language only if it is not already in the
    // accept language list.
    std::string language =
        dictionary_language_model_->GetLocaleFromIndex(new_index);
    int index = language_order_table_model_->GetIndex(language);
    if (index == -1) {
      // Add the new language.
      OnAddLanguage(language);
      language_table_edited_ = true;
      spellcheck_language_added_ = language;
    } else {
      spellcheck_language_added_ = "";
    }
  }
}

void LanguagesPageView::OnSelectionChanged() {
  move_up_button_->SetEnabled(language_order_table_->FirstSelectedRow() > 0 &&
                              language_order_table_->SelectedRowCount() == 1);
  move_down_button_->SetEnabled(language_order_table_->FirstSelectedRow() <
                                language_order_table_->RowCount() - 1 &&
                                language_order_table_->SelectedRowCount() ==
                                1);
  remove_button_->SetEnabled(language_order_table_->SelectedRowCount() > 0);
}

void LanguagesPageView::OnRemoveLanguage() {
  int item_selected = 0;
  for (views::TableView::iterator i =
       language_order_table_->SelectionBegin();
       i != language_order_table_->SelectionEnd(); ++i) {
    language_order_table_model_->Remove(*i);
    item_selected = *i;
  }

  move_up_button_->SetEnabled(false);
  move_down_button_->SetEnabled(false);
  remove_button_->SetEnabled(false);
  int items_left = language_order_table_model_->RowCount();
  if (items_left <= 0)
    return;
  if (item_selected > items_left - 1)
    item_selected = items_left - 1;
  language_order_table_->Select(item_selected);
  OnSelectionChanged();
}

void LanguagesPageView::OnMoveDownLanguage() {
  int item_selected = language_order_table_->FirstSelectedRow();
  language_order_table_model_->MoveDown(item_selected);
  language_order_table_->Select(item_selected + 1);
  OnSelectionChanged();
}

void LanguagesPageView::OnMoveUpLanguage() {
  int item_selected = language_order_table_->FirstSelectedRow();
  language_order_table_model_->MoveUp(item_selected);
  language_order_table_->Select(item_selected - 1);

  OnSelectionChanged();
}

void LanguagesPageView::SaveChanges() {
  if (language_order_table_model_.get() && language_table_edited_) {
    accept_languages_.SetValue(ASCIIToWide(
        language_order_table_model_->GetLanguageList()));
  }

  if (ui_language_index_selected_ != -1) {
    UserMetricsRecordAction(L"Options_AppLanguage",
                            g_browser_process->local_state());
    app_locale_.SetValue(ASCIIToWide(ui_language_model_->
        GetLocaleFromIndex(ui_language_index_selected_)));

    // Remove pref values for spellcheck dictionaries forcefully.
    PrefService* prefs = profile()->GetPrefs();
    if (prefs)
      prefs->ClearPref(prefs::kSpellCheckDictionary);
  }

  if (spellcheck_language_index_selected_ != -1) {
    UserMetricsRecordAction(L"Options_DictionaryLanguage",
                            profile()->GetPrefs());
    dictionary_language_.SetValue(ASCIIToWide(dictionary_language_model_->
        GetLocaleFromIndex(spellcheck_language_index_selected_)));
  }
}
