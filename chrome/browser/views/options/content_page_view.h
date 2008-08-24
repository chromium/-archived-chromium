// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_OPTIONS_CONTENT_PAGE_VIEW_H__
#define CHROME_BROWSER_VIEWS_OPTIONS_CONTENT_PAGE_VIEW_H__

#include "chrome/browser/views/options/options_page_view.h"
#include "chrome/browser/shell_dialogs.h"
#include "chrome/common/pref_member.h"
#include "chrome/views/native_button.h"
#include "chrome/views/view.h"

namespace ChromeViews {
class CheckBox;
class RadioButton;
}
class FileDisplayArea;
class OptionsGroupView;
class PrefService;

////////////////////////////////////////////////////////////////////////////////
// ContentPageView

class ContentPageView : public OptionsPageView,
                        public ChromeViews::NativeButton::Listener,
                        public SelectFileDialog::Listener {
 public:
  explicit ContentPageView(Profile* profile);
  virtual ~ContentPageView();

  // ChromeViews::NativeButton::Listener implementation:
  virtual void ButtonPressed(ChromeViews::NativeButton* sender);

  // SelectFileDialog::Listener implementation:
  virtual void FileSelected(const std::wstring& path, void* params);

  // OptionsPageView implementation:
  virtual bool CanClose() const;

 protected:
  // OptionsPageView implementation:
  virtual void InitControlLayout();
  virtual void NotifyPrefChanged(const std::wstring* pref_name);

  // ChromeViews::View overrides:
  virtual void Layout();

 private:
  // Init all the dialog controls.
  void InitDownloadLocation();
  void InitPasswordSavingGroup();
  void InitFontsLangGroup();

  // Updates the directory displayed in the default download location view with
  // the current value of the pref.
  void UpdateDownloadDirectoryDisplay();

  // Controls for the Download Location group.
  OptionsGroupView* download_location_group_;
  FileDisplayArea* download_default_download_location_display_;
  ChromeViews::NativeButton* download_browse_button_;
  ChromeViews::CheckBox* download_ask_for_save_location_checkbox_;
  scoped_refptr<SelectFileDialog> select_file_dialog_;

  // Controls for the Password Saving group
  OptionsGroupView* passwords_group_;
  ChromeViews::RadioButton* passwords_asktosave_radio_;
  ChromeViews::RadioButton* passwords_neversave_radio_;
  ChromeViews::NativeButton* passwords_show_passwords_button_;

  // Controls for the Popup Blocking group.
  OptionsGroupView* popups_group_;
  ChromeViews::RadioButton* popups_show_minimized_radio_;
  ChromeViews::RadioButton* popups_block_all_radio_;

  // Controls for the Fonts and Languages group.
  OptionsGroupView* fonts_lang_group_;
  ChromeViews::Label* fonts_and_languages_label_;
  ChromeViews::NativeButton* change_content_fonts_button_;

  StringPrefMember default_download_location_;
  BooleanPrefMember ask_for_save_location_;
  BooleanPrefMember ask_to_save_passwords_;

  DISALLOW_EVIL_CONSTRUCTORS(ContentPageView);
};

#endif  // #ifndef CHROME_BROWSER_VIEWS_OPTIONS_CONTENT_PAGE_VIEW_H__

