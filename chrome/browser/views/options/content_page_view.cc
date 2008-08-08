// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <windows.h>
#include <shlobj.h>
#include <vsstyle.h>
#include <vssym32.h>

#include "chrome/browser/views/options/content_page_view.h"

#include "base/file_util.h"
#include "base/gfx/native_theme.h"
#include "base/gfx/skia_utils.h"
#include "chrome/app/theme/theme_resources.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/shell_dialogs.h"
#include "chrome/browser/standard_layout.h"
#include "chrome/browser/views/options/fonts_languages_window_view.h"
#include "chrome/browser/views/options/options_group_view.h"
#include "chrome/browser/views/password_manager_view.h"
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
#include "chrome/views/view_container.h"
#include "generated_resources.h"
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

class FileDisplayArea : public ChromeViews::View {
 public:
  FileDisplayArea();
  virtual ~FileDisplayArea();

  void SetFile(const std::wstring& file_path);

  // ChromeViews::View overrides:
  virtual void Paint(ChromeCanvas* canvas);
  virtual void Layout();
  virtual void GetPreferredSize(CSize* out);

 protected:
  // ChromeViews::View overrides:
  virtual void ViewHierarchyChanged(bool is_add,
                                    ChromeViews::View* parent,
                                    ChromeViews::View* child);

 private:
  void Init();

  ChromeViews::TextField* text_field_;
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
    : text_field_(new ChromeViews::TextField),
      text_field_background_color_(0),
      initialized_(false) {
  InitClass();
}

FileDisplayArea::~FileDisplayArea() {
}

void FileDisplayArea::SetFile(const std::wstring& file_path) {
  text_field_->SetText(file_path);
}

void FileDisplayArea::Paint(ChromeCanvas* canvas) {
  HDC dc = canvas->beginPlatformPaint();
  RECT rect = { 0, 0, GetWidth(), GetHeight() };
  gfx::NativeTheme::instance()->PaintTextField(
      dc, EP_EDITTEXT, ETS_READONLY, 0, &rect,
      gfx::SkColorToCOLORREF(text_field_background_color_), true, true);
  canvas->endPlatformPaint();
  canvas->DrawBitmapInt(default_folder_icon_, icon_bounds_.x(),
                        icon_bounds_.y());
}

void FileDisplayArea::Layout() {
  icon_bounds_.SetRect(kFileIconHorizontalSpacing, kFileIconVerticalSpacing,
                       kFileIconSize, kFileIconSize);
  CSize ps;
  text_field_->GetPreferredSize(&ps);
  text_field_->SetBounds(icon_bounds_.right() + kFileIconTextFieldSpacing,
                         (GetHeight() - ps.cy) / 2,
                         GetWidth() - icon_bounds_.right() -
                             kFileIconHorizontalSpacing -
                             kFileIconTextFieldSpacing, ps.cy);
}

void FileDisplayArea::GetPreferredSize(CSize* out) {
  DCHECK(out);
  out->cx = kFileIconSize + 2 * kFileIconVerticalSpacing;
  out->cy = kFileIconSize + 2 * kFileIconHorizontalSpacing;
}

void FileDisplayArea::ViewHierarchyChanged(bool is_add,
                                           ChromeViews::View* parent,
                                           ChromeViews::View* child) {
  if (!initialized_ && is_add && GetViewContainer())
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

void FileDisplayArea::InitClass() {
  static bool initialized = false;
  if (!initialized) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    default_folder_icon_ = *rb.GetBitmapNamed(IDR_FOLDER_CLOSED);
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
// ContentPageView, ChromeViews::NativeButton::Listener implementation:

void ContentPageView::ButtonPressed(ChromeViews::NativeButton* sender) {
  if (sender == download_browse_button_) {
    const std::wstring dialog_title =
       l10n_util::GetString(IDS_OPTIONS_DOWNLOADLOCATION_BROWSE_TITLE);
    select_file_dialog_->SelectFile(SelectFileDialog::SELECT_FOLDER,
                                    dialog_title, L"", GetRootWindow(), NULL);
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
  } else if (sender == passwords_show_passwords_button_) {
    UserMetricsRecordAction(L"Options_ShowPasswordManager", NULL);
    PasswordManagerView::Show(profile());
  } else if (sender == change_content_fonts_button_) {
    ChromeViews::Window::CreateChromeWindow(
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
  using ChromeViews::GridLayout;
  using ChromeViews::ColumnSet;

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

  // Init member prefs so we can update the controls if prefs change.
  default_download_location_.Init(prefs::kDownloadDefaultDirectory,
                                  profile()->GetPrefs(), this);
  ask_for_save_location_.Init(prefs::kPromptForDownload,
                              profile()->GetPrefs(), this);
  ask_to_save_passwords_.Init(prefs::kPasswordManagerEnabled,
                              profile()->GetPrefs(), this);
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
}

///////////////////////////////////////////////////////////////////////////////
// ContentsPageView, ChromeViews::View overrides:

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
  download_browse_button_ = new ChromeViews::NativeButton(
      l10n_util::GetString(IDS_OPTIONS_DOWNLOADLOCATION_BROWSE_BUTTON));
  download_browse_button_->SetListener(this);

  download_ask_for_save_location_checkbox_ = new ChromeViews::CheckBox(
      l10n_util::GetString(IDS_OPTIONS_DOWNLOADLOCATION_ASKFORSAVELOCATION));
  download_ask_for_save_location_checkbox_->SetListener(this);
  download_ask_for_save_location_checkbox_->SetMultiLine(true);

  using ChromeViews::GridLayout;
  using ChromeViews::ColumnSet;

  ChromeViews::View* contents = new ChromeViews::View;
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
  passwords_asktosave_radio_ = new ChromeViews::RadioButton(
      l10n_util::GetString(IDS_OPTIONS_PASSWORDS_ASKTOSAVE),
      kPasswordSavingRadioGroup);
  passwords_asktosave_radio_->SetListener(this);
  passwords_asktosave_radio_->SetMultiLine(true);
  passwords_neversave_radio_ = new ChromeViews::RadioButton(
      l10n_util::GetString(IDS_OPTIONS_PASSWORDS_NEVERSAVE),
      kPasswordSavingRadioGroup);
  passwords_neversave_radio_->SetListener(this);
  passwords_neversave_radio_->SetMultiLine(true);
  passwords_show_passwords_button_ = new ChromeViews::NativeButton(
      l10n_util::GetString(IDS_OPTIONS_PASSWORDS_SHOWPASSWORDS));
  passwords_show_passwords_button_->SetListener(this);

  using ChromeViews::GridLayout;
  using ChromeViews::ColumnSet;

  ChromeViews::View* contents = new ChromeViews::View;
  GridLayout* layout = new GridLayout(contents);
  contents->SetLayoutManager(layout);

  const int single_column_view_set_id = 1;
  ColumnSet* column_set = layout->AddColumnSet(single_column_view_set_id);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 1,
                        GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(passwords_asktosave_radio_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(passwords_neversave_radio_);
  layout->AddPaddingRow(0, kUnrelatedControlVerticalSpacing);
  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(passwords_show_passwords_button_);

  passwords_group_ = new OptionsGroupView(
      contents, l10n_util::GetString(IDS_OPTIONS_PASSWORDS_GROUP_NAME), L"",
      true);
}

void ContentPageView::InitFontsLangGroup() {
  fonts_and_languages_label_ = new ChromeViews::Label(
      l10n_util::GetString(IDS_OPTIONS_FONTSETTINGS_INFO));
  fonts_and_languages_label_->SetHorizontalAlignment(
      ChromeViews::Label::ALIGN_LEFT);
  fonts_and_languages_label_->SetMultiLine(true);
  change_content_fonts_button_ = new ChromeViews::NativeButton(
      l10n_util::GetString(IDS_OPTIONS_FONTSETTINGS_CONFIGUREFONTS_BUTTON));
  change_content_fonts_button_->SetListener(this);

  using ChromeViews::GridLayout;
  using ChromeViews::ColumnSet;

  ChromeViews::View* contents = new ChromeViews::View;
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
      L"", false);
}

void ContentPageView::UpdateDownloadDirectoryDisplay() {
  download_default_download_location_display_->SetFile(
      default_download_location_.GetValue());
}
