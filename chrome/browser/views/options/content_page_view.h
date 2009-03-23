// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_OPTIONS_CONTENT_PAGE_VIEW_H__
#define CHROME_BROWSER_VIEWS_OPTIONS_CONTENT_PAGE_VIEW_H__

#include "chrome/browser/views/options/options_page_view.h"
#include "chrome/browser/shell_dialogs.h"
#include "chrome/common/pref_member.h"
#include "chrome/views/controls/button/button.h"
#include "chrome/views/view.h"

namespace views {
class Checkbox;
class NativeButton;
class RadioButton;
}
class FileDisplayArea;
class OptionsGroupView;
class PrefService;

////////////////////////////////////////////////////////////////////////////////
// ContentPageView

class ContentPageView : public OptionsPageView,
                        public views::ButtonListener,
                        public SelectFileDialog::Listener {
 public:
  explicit ContentPageView(Profile* profile);
  virtual ~ContentPageView();

  // views::ButtonListener implementation:
  virtual void ButtonPressed(views::Button* sender);

  // SelectFileDialog::Listener implementation:
  virtual void FileSelected(const std::wstring& path, void* params);

  // OptionsPageView implementation:
  virtual bool CanClose() const;

 protected:
  // OptionsPageView implementation:
  virtual void InitControlLayout();
  virtual void NotifyPrefChanged(const std::wstring* pref_name);

  // views::View overrides:
  virtual void Layout();

 private:
  // Init all the dialog controls.
  void InitDownloadLocation();
  void InitPasswordSavingGroup();
  void InitFormAutofillGroup();
  void InitFontsLangGroup();

  // Updates the directory displayed in the default download location view with
  // the current value of the pref.
  void UpdateDownloadDirectoryDisplay();

  // Controls for the Download Location group.
  OptionsGroupView* download_location_group_;
  FileDisplayArea* download_default_download_location_display_;
  views::NativeButton* download_browse_button_;
  views::Checkbox* download_ask_for_save_location_checkbox_;
  scoped_refptr<SelectFileDialog> select_file_dialog_;

  // Controls for the Password Saving group
  views::NativeButton* passwords_exceptions_button_;
  OptionsGroupView* passwords_group_;
  views::RadioButton* passwords_asktosave_radio_;
  views::RadioButton* passwords_neversave_radio_;
  views::NativeButton* passwords_show_passwords_button_;

  // Controls for the Form Autofill group
  OptionsGroupView* form_autofill_group_;
  views::Checkbox* form_autofill_checkbox_;

  // Controls for the Popup Blocking group.
  OptionsGroupView* popups_group_;
  views::RadioButton* popups_show_minimized_radio_;
  views::RadioButton* popups_block_all_radio_;

  // Controls for the Fonts and Languages group.
  OptionsGroupView* fonts_lang_group_;
  views::Label* fonts_and_languages_label_;
  views::NativeButton* change_content_fonts_button_;

  StringPrefMember default_download_location_;
  BooleanPrefMember ask_for_save_location_;
  BooleanPrefMember ask_to_save_passwords_;
  BooleanPrefMember form_autofill_;

  DISALLOW_EVIL_CONSTRUCTORS(ContentPageView);
};

#endif  // #ifndef CHROME_BROWSER_VIEWS_OPTIONS_CONTENT_PAGE_VIEW_H__
