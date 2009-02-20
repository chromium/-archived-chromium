// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <shlobj.h>
#include <vsstyle.h>
#include <vssym32.h>

#include "chrome/browser/views/options/content_page_view.h"

#include "base/file_util.h"
#include "base/gfx/native_theme.h"
#include "grit/theme_resources.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/shell_dialogs.h"
#include "chrome/browser/views/options/fonts_languages_window_view.h"
#include "chrome/browser/views/options/options_group_view.h"
#include "chrome/browser/views/password_manager_view.h"
#include "chrome/browser/views/password_manager_exceptions_view.h"
#include "chrome/browser/views/standard_layout.h"
#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/views/checkbox.h"
#include "chrome/views/grid_layout.h"
#include "chrome/views/native_button.h"
#include "chrome/views/radio_button.h"
#include "chrome/views/text_field.h"
#include "chrome/views/widget.h"
#include "generated_resources.h"
#include "skia/ext/skia_utils_win.h"
#include "skia/include/SkBitmap.h"

namespace {

static const int kPopupBlockingRadioGroup = 1;
static const int kPasswordSavingRadioGroup = 2;
static const int kFileIconSize = 16;
static const int kFileIconVerticalSpacing = 3;
static const int kFileIconHorizontalSpacing = 3;
static const int kFileIconTextFieldSpacing = 3;
}  // namespace

////////////////////////////////////////////////////////////////////////////////
// FileDisplayArea

class FileDisplayArea : public views::View {
 public:
  FileDisplayArea();
  virtual ~FileDisplayArea();

  void SetFile(const FilePath& file_path);

  // views::View overrides:
  virtual void Paint(ChromeCanvas* canvas);
  virtual void Layout();
  virtual gfx::Size GetPreferredSize();

 protected:
  // views::View overrides:
  virtual void ViewHierarchyChanged(bool is_add,
                                    views::View* parent,
                                    views::View* child);

 private:
  void Init();

  views::TextField* text_field_;
  SkColor text_field_background_color_;

  gfx::Rect icon_bounds_;

  bool initialized_;

  static void InitClass();
  static SkBitmap default_folder_icon_;

  DISALLOW_EVIL_CONSTRUCTORS(FileDisplayArea);
};

// static
SkBitmap FileDisplayArea::default_folder_icon_;

FileDisplayArea::FileDisplayArea()
    : text_field_(new views::TextField),
      text_field_background_color_(0),
      initialized_(false) {
  InitClass();
}

FileDisplayArea::~FileDisplayArea() {
}

void FileDisplayArea::SetFile(const FilePath& file_path) {
  // Force file path to have LTR directionality.
  if (l10n_util::GetTextDirection() == l10n_util::RIGHT_TO_LEFT) {
    string16 localized_file_path;
    l10n_util::WrapPathWithLTRFormatting(file_path, &localized_file_path);
    text_field_->SetText(UTF16ToWide(localized_file_path));
  } else {
    text_field_->SetText(file_path.ToWStringHack());
  }
}

void FileDisplayArea::Paint(ChromeCanvas* canvas) {
  HDC dc = canvas->beginPlatformPaint();
  RECT rect = { 0, 0, width(), height() };
  gfx::NativeTheme::instance()->PaintTextField(
      dc, EP_EDITTEXT, ETS_READONLY, 0, &rect,
      skia::SkColorToCOLORREF(text_field_background_color_), true, true);
  canvas->endPlatformPaint();
  // Mirror left point for icon_bounds_ to draw icon in RTL locales correctly.
  canvas->DrawBitmapInt(default_folder_icon_,
                        MirroredLeftPointForRect(icon_bounds_),
                        icon_bounds_.y());
}

void FileDisplayArea::Layout() {
  icon_bounds_.SetRect(kFileIconHorizontalSpacing, kFileIconVerticalSpacing,
                       kFileIconSize, kFileIconSize);
  gfx::Size ps = text_field_->GetPreferredSize();
  text_field_->SetBounds(icon_bounds_.right() + kFileIconTextFieldSpacing,
                         (height() - ps.height()) / 2,
                         width() - icon_bounds_.right() -
                             kFileIconHorizontalSpacing -
                             kFileIconTextFieldSpacing, ps.height());
}

gfx::Size FileDisplayArea::GetPreferredSize() {
  return gfx::Size(kFileIconSize + 2 * kFileIconVerticalSpacing,
                   kFileIconSize + 2 * kFileIconHorizontalSpacing);
}

void FileDisplayArea::ViewHierarchyChanged(bool is_add,
                                           views::View* parent,
                                           views::View* child) {
  if (!initialized_ && is_add && GetWidget())
    Init();
}

void FileDisplayArea::Init() {
  initialized_ = true;
  AddChildView(text_field_);
  text_field_background_color_ =
      gfx::NativeTheme::instance()->GetThemeColorWithDefault(
          gfx::NativeTheme::TEXTFIELD, EP_EDITTEXT, ETS_READONLY,
          TMT_FILLCOLOR, COLOR_3DFACE);
  text_field_->SetReadOnly(true);
  text_field_->RemoveBorder();
  text_field_->SetBackgroundColor(text_field_background_color_);
}

// static
void FileDisplayArea::InitClass() {
  static bool initialized = false;
  if (!initialized) {
    // We'd prefer to use UILayoutIsRightToLeft() to perform the RTL
    // environment check, but it's nonstatic, so, instead, we check whether the
    // locale is RTL.
    bool ui_is_rtl = l10n_util::GetTextDirection() == l10n_util::RIGHT_TO_LEFT;
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    default_folder_icon_ = *rb.GetBitmapNamed(ui_is_rtl ?
                                              IDR_FOLDER_CLOSED_RTL :
                                              IDR_FOLDER_CLOSED);
    initialized = true;
  }
}

////////////////////////////////////////////////////////////////////////////////
// ContentPageView, public:

ContentPageView::ContentPageView(Profile* profile)
    : download_location_group_(NULL),
      download_default_download_location_display_(NULL),
      download_browse_button_(NULL),
      download_ask_for_save_location_checkbox_(NULL),
      select_file_dialog_(SelectFileDialog::Create(this)),
      passwords_exceptions_button_(NULL),
      passwords_group_(NULL),
      passwords_asktosave_radio_(NULL),
      passwords_neversave_radio_(NULL),
      passwords_show_passwords_button_(NULL),
      fonts_lang_group_(NULL),
      fonts_and_languages_label_(NULL),
      change_content_fonts_button_(NULL),
      OptionsPageView(profile) {
}

ContentPageView::~ContentPageView() {
  select_file_dialog_->ListenerDestroyed();
}

////////////////////////////////////////////////////////////////////////////////
// ContentPageView, SelectFileDialog::Listener implementation:

void ContentPageView::FileSelected(const std::wstring& path, void* params) {
  UserMetricsRecordAction(L"Options_SetDownloadDirectory",
                          profile()->GetPrefs());
  default_download_location_.SetValue(path);
  // We need to call this manually here since because we're setting the value
  // through the pref member which avoids notifying the listener that set the
  // value.
  UpdateDownloadDirectoryDisplay();
}

///////////////////////////////////////////////////////////////////////////////
// ContentPageView, views::NativeButton::Listener implementation:

void ContentPageView::ButtonPressed(views::NativeButton* sender) {
  if (sender == download_browse_button_) {
    const std::wstring dialog_title =
       l10n_util::GetString(IDS_OPTIONS_DOWNLOADLOCATION_BROWSE_TITLE);
    select_file_dialog_->SelectFile(SelectFileDialog::SELECT_FOLDER,
                                    dialog_title,
                                    profile()->GetPrefs()->GetString(
                                        prefs::kDownloadDefaultDirectory),
                                    std::wstring(), std::wstring(),
                                    GetRootWindow(),
                                    NULL);
  } else if (sender == download_ask_for_save_location_checkbox_) {
    bool enabled = download_ask_for_save_location_checkbox_->IsSelected();
    if (enabled) {
      UserMetricsRecordAction(L"Options_AskForSaveLocation_Enable",
                              profile()->GetPrefs());
    } else {
      UserMetricsRecordAction(L"Options_AskForSaveLocation_Disable",
                              profile()->GetPrefs());
    }
    ask_for_save_location_.SetValue(enabled);
  } else if (sender == passwords_asktosave_radio_ ||
             sender == passwords_neversave_radio_) {
    bool enabled = passwords_asktosave_radio_->IsSelected();
    if (enabled) {
      UserMetricsRecordAction(L"Options_PasswordManager_Enable",
                              profile()->GetPrefs());
    } else {
      UserMetricsRecordAction(L"Options_PasswordManager_Disable",
                              profile()->GetPrefs());
    }
    ask_to_save_passwords_.SetValue(enabled);
  } else if (sender == passwords_exceptions_button_) {
    UserMetricsRecordAction(L"Options_ShowPasswordManagerExceptions", NULL);
    PasswordManagerExceptionsView::Show(profile());
  }else if (sender == passwords_show_passwords_button_) {
    UserMetricsRecordAction(L"Options_ShowPasswordManager", NULL);
    PasswordManagerView::Show(profile());
  } else if (sender == form_autofill_checkbox_) {
    bool enabled = form_autofill_checkbox_->IsSelected();
    if (enabled) {
      UserMetricsRecordAction(L"Options_FormAutofill_Enable",
                              profile()->GetPrefs());
    } else {
      UserMetricsRecordAction(L"Options_FormAutofill_Disable",
                              profile()->GetPrefs());
    }
    form_autofill_.SetValue(enabled);
  } else if (sender == change_content_fonts_button_) {
    views::Window::CreateChromeWindow(
        GetRootWindow(),
        gfx::Rect(),
        new FontsLanguagesWindowView(profile()))->Show();
  }
}

////////////////////////////////////////////////////////////////////////////////
// ContentPageView, OptionsPageView implementation:

bool ContentPageView::CanClose() const {
  return !select_file_dialog_->IsRunning(GetRootWindow());
}

void ContentPageView::InitControlLayout() {
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
  InitDownloadLocation();
  layout->AddView(download_location_group_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  layout->StartRow(0, single_column_view_set_id);
  InitPasswordSavingGroup();
  layout->AddView(passwords_group_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  layout->StartRow(0, single_column_view_set_id);
  InitFontsLangGroup();
  layout->AddView(fonts_lang_group_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  layout->StartRow(0, single_column_view_set_id);
  InitFormAutofillGroup();
  layout->AddView(form_autofill_group_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  // Init member prefs so we can update the controls if prefs change.
  default_download_location_.Init(prefs::kDownloadDefaultDirectory,
                                  profile()->GetPrefs(), this);
  ask_for_save_location_.Init(prefs::kPromptForDownload,
                              profile()->GetPrefs(), this);
  ask_to_save_passwords_.Init(prefs::kPasswordManagerEnabled,
                              profile()->GetPrefs(), this);
  form_autofill_.Init(prefs::kFormAutofillEnabled, profile()->GetPrefs(), this);
}

void ContentPageView::NotifyPrefChanged(const std::wstring* pref_name) {
  if (!pref_name || *pref_name == prefs::kDownloadDefaultDirectory)
    UpdateDownloadDirectoryDisplay();

  if (!pref_name || *pref_name == prefs::kPromptForDownload) {
    download_ask_for_save_location_checkbox_->SetIsSelected(
        ask_for_save_location_.GetValue());
  }
  if (!pref_name || *pref_name == prefs::kPasswordManagerEnabled) {
    if (ask_to_save_passwords_.GetValue()) {
      passwords_asktosave_radio_->SetIsSelected(true);
    } else {
      passwords_neversave_radio_->SetIsSelected(true);
    }
  }
  if (!pref_name || *pref_name == prefs::kFormAutofillEnabled) {
    form_autofill_checkbox_->SetIsSelected(form_autofill_.GetValue());
  }
}

///////////////////////////////////////////////////////////////////////////////
// ContentsPageView, views::View overrides:

void ContentPageView::Layout() {
  // We need to Layout twice - once to get the width of the contents box...
  View::Layout();
  download_ask_for_save_location_checkbox_->SetBounds(
      0, 0, download_location_group_->GetContentsWidth(), 0);
  passwords_asktosave_radio_->SetBounds(
      0, 0, passwords_group_->GetContentsWidth(), 0);
  passwords_neversave_radio_->SetBounds(
      0, 0, passwords_group_->GetContentsWidth(), 0);
  fonts_and_languages_label_->SetBounds(
      0, 0, fonts_lang_group_->GetContentsWidth(), 0);
  // ... and twice to get the height of multi-line items correct.
  View::Layout();
}

///////////////////////////////////////////////////////////////////////////////
// ContentPageView, private:

void ContentPageView::InitDownloadLocation() {
  download_default_download_location_display_ = new FileDisplayArea;
  download_browse_button_ = new views::NativeButton(
      l10n_util::GetString(IDS_OPTIONS_DOWNLOADLOCATION_BROWSE_BUTTON));
  download_browse_button_->SetListener(this);

  download_ask_for_save_location_checkbox_ = new views::CheckBox(
      l10n_util::GetString(IDS_OPTIONS_DOWNLOADLOCATION_ASKFORSAVELOCATION));
  download_ask_for_save_location_checkbox_->SetListener(this);
  download_ask_for_save_location_checkbox_->SetMultiLine(true);

  using views::GridLayout;
  using views::ColumnSet;

  views::View* contents = new views::View;
  GridLayout* layout = new GridLayout(contents);
  contents->SetLayoutManager(layout);

  const int double_column_view_set_id = 0;
  ColumnSet* column_set = layout->AddColumnSet(double_column_view_set_id);
  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 1,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kUnrelatedControlHorizontalSpacing);

  layout->StartRow(0, double_column_view_set_id);
  layout->AddView(download_default_download_location_display_, 1, 1,
                  GridLayout::FILL, GridLayout::CENTER);
  layout->AddView(download_browse_button_);

  layout->AddPaddingRow(0, kUnrelatedControlVerticalSpacing);

  const int single_column_view_set_id = 1;
  column_set = layout->AddColumnSet(single_column_view_set_id);
  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 1,
                        GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(download_ask_for_save_location_checkbox_);

  download_location_group_ = new OptionsGroupView(
      contents, l10n_util::GetString(IDS_OPTIONS_DOWNLOADLOCATION_GROUP_NAME),
      std::wstring(),
      true);
}

void ContentPageView::InitPasswordSavingGroup() {
  passwords_asktosave_radio_ = new views::RadioButton(
      l10n_util::GetString(IDS_OPTIONS_PASSWORDS_ASKTOSAVE),
      kPasswordSavingRadioGroup);
  passwords_asktosave_radio_->SetListener(this);
  passwords_asktosave_radio_->SetMultiLine(true);
  passwords_neversave_radio_ = new views::RadioButton(
      l10n_util::GetString(IDS_OPTIONS_PASSWORDS_NEVERSAVE),
      kPasswordSavingRadioGroup);
  passwords_neversave_radio_->SetListener(this);
  passwords_neversave_radio_->SetMultiLine(true);
  passwords_show_passwords_button_ = new views::NativeButton(
      l10n_util::GetString(IDS_OPTIONS_PASSWORDS_SHOWPASSWORDS));
  passwords_show_passwords_button_->SetListener(this);
  passwords_exceptions_button_ = new views::NativeButton(
      l10n_util::GetString(IDS_OPTIONS_PASSWORDS_EXCEPTIONS));
  passwords_exceptions_button_->SetListener(this);

  using views::GridLayout;
  using views::ColumnSet;

  views::View* contents = new views::View;
  GridLayout* layout = new GridLayout(contents);
  contents->SetLayoutManager(layout);

  const int single_column_view_set_id = 1;
  ColumnSet* column_set = layout->AddColumnSet(single_column_view_set_id);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 1,
                        GridLayout::USE_PREF, 0, 0);

  const int double_column_view_set_id = 0;
  column_set = layout->AddColumnSet(double_column_view_set_id);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(passwords_asktosave_radio_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(passwords_neversave_radio_);
  layout->AddPaddingRow(0, kUnrelatedControlVerticalSpacing);
  layout->StartRow(0, double_column_view_set_id);
  layout->AddView(passwords_show_passwords_button_);
  layout->AddView(passwords_exceptions_button_);

  passwords_group_ = new OptionsGroupView(
      contents, l10n_util::GetString(IDS_OPTIONS_PASSWORDS_GROUP_NAME), L"",
      true);
}

void ContentPageView::InitFormAutofillGroup() {
  form_autofill_checkbox_ = new views::CheckBox(
      l10n_util::GetString(IDS_AUTOFILL_SAVEFORMS));
  form_autofill_checkbox_->SetListener(this);
  form_autofill_checkbox_->SetMultiLine(true);

  using views::GridLayout;
  using views::ColumnSet;

  views::View* contents = new views::View;
  GridLayout* layout = new GridLayout(contents);
  contents->SetLayoutManager(layout);

  const int single_column_view_set_id = 1;
  ColumnSet* column_set = layout->AddColumnSet(single_column_view_set_id);
  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 1,
                        GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(form_autofill_checkbox_);

  form_autofill_group_ = new OptionsGroupView(
      contents, l10n_util::GetString(IDS_AUTOFILL_SETTING_WINDOWS_GROUP_NAME),
      L"", false);
}

void ContentPageView::InitFontsLangGroup() {
  fonts_and_languages_label_ = new views::Label(
      l10n_util::GetString(IDS_OPTIONS_FONTSETTINGS_INFO));
  fonts_and_languages_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  fonts_and_languages_label_->SetMultiLine(true);
  change_content_fonts_button_ = new views::NativeButton(
      l10n_util::GetString(IDS_OPTIONS_FONTSETTINGS_CONFIGUREFONTS_BUTTON));
  change_content_fonts_button_->SetListener(this);

  using views::GridLayout;
  using views::ColumnSet;

  views::View* contents = new views::View;
  GridLayout* layout = new GridLayout(contents);
  contents->SetLayoutManager(layout);

  const int single_column_view_set_id = 1;
  ColumnSet* column_set = layout->AddColumnSet(single_column_view_set_id);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 1,
                        GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(fonts_and_languages_label_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(change_content_fonts_button_);

  fonts_lang_group_ = new OptionsGroupView(
      contents,
      l10n_util::GetString(IDS_OPTIONS_FONTSANDLANGUAGES_GROUP_NAME),
      L"", true);
}

void ContentPageView::UpdateDownloadDirectoryDisplay() {
  download_default_download_location_display_->SetFile(
      FilePath::FromWStringHack(default_download_location_.GetValue()));
}

