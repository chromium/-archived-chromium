// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_OPTIONS_CONTENT_PAGE_VIEW_H_
#define CHROME_BROWSER_VIEWS_OPTIONS_CONTENT_PAGE_VIEW_H_

#include "chrome/browser/views/options/options_page_view.h"
#include "chrome/common/pref_member.h"
#include "views/controls/button/button.h"
#include "views/view.h"

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
                        public views::ButtonListener {
 public:
  explicit ContentPageView(Profile* profile);
  virtual ~ContentPageView();

  // views::ButtonListener implementation:
  virtual void ButtonPressed(views::Button* sender);

 protected:
  // OptionsPageView implementation:
  virtual void InitControlLayout();
  virtual void NotifyPrefChanged(const std::wstring* pref_name);

  // views::View overrides:
  virtual void Layout();

 private:
  // Init all the dialog controls.
  void InitPasswordSavingGroup();
  void InitFormAutofillGroup();
  void InitBrowsingDataGroup();
  void InitThemesGroup();

  // Controls for the Password Saving group
  views::NativeButton* passwords_exceptions_button_;
  OptionsGroupView* passwords_group_;
  views::RadioButton* passwords_asktosave_radio_;
  views::RadioButton* passwords_neversave_radio_;

  // Controls for the Form Autofill group
  OptionsGroupView* form_autofill_group_;
  views::RadioButton* form_autofill_asktosave_radio_;
  views::RadioButton* form_autofill_neversave_radio_;

  // Controls for the Themes group
  OptionsGroupView* themes_group_;
  views::NativeButton* themes_reset_button_;

  // Controls for the browsing data group.
  OptionsGroupView* browsing_data_group_;
  views::Label* browsing_data_label_;
  views::NativeButton* import_button_;
  views::NativeButton* clear_data_button_;

  BooleanPrefMember ask_to_save_passwords_;
  BooleanPrefMember ask_to_save_form_autofill_;

  DISALLOW_COPY_AND_ASSIGN(ContentPageView);
};

#endif  // CHROME_BROWSER_VIEWS_OPTIONS_CONTENT_PAGE_VIEW_H_
