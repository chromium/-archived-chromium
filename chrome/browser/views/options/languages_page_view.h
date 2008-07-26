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

#ifndef CHROME_BROWSER_VIEWS_OPTIONS_LANGUAGES_PAGE_VIEW_H__
#define CHROME_BROWSER_VIEWS_OPTIONS_LANGUAGES_PAGE_VIEW_H__

#include "chrome/browser/views/options/options_page_view.h"
#include "chrome/common/pref_member.h"
#include "chrome/views/combo_box.h"
#include "chrome/views/native_button.h"
#include "chrome/views/table_view.h"
#include "chrome/views/view.h"

namespace ChromeViews {
class Label;
class TableModel;
class TableView;
}

class LanguageComboboxModel;
class LanguageOrderTableModel;
class AddLanguageView;

///////////////////////////////////////////////////////////////////////////////
// LanguagesPageView

class LanguagesPageView : public OptionsPageView,
                          public ChromeViews::NativeButton::Listener,
                          public ChromeViews::TableViewObserver,
                          public ChromeViews::ComboBox::Listener {
 public:
  explicit LanguagesPageView(Profile* profile);
  virtual ~LanguagesPageView();

  // ChromeViews::NativeButton::Listener implementation:
  virtual void ButtonPressed(ChromeViews::NativeButton* sender);

  // Save Changes made to relevant pref members associated with this tab.
  // This is public since it is called by FontsLanguageWindowView in its
  // Dialog Delegate Accept() method.
  void SaveChanges();

  // This is public because when user clicks OK in AddLanguageView dialog,
  // this is called back in the LanguagePageView delegate in order to add
  // this language to the table model in this tab.
  void OnAddLanguage(const std::wstring& new_language);

 protected:
  // OptionsPageView implementation:
  virtual void InitControlLayout();
  virtual void NotifyPrefChanged(const std::wstring* pref_name);

  // ChromeViews::ComboBox::Listener implementation:
  virtual void ItemChanged(ChromeViews::ComboBox* sender,
                           int prev_index,
                           int new_index);

 private:
  // Invoked when the selection of the table view changes. Updates the enabled
  // property of the remove button.
  virtual void OnSelectionChanged();
  void OnRemoveLanguage();
  void OnMoveDownLanguage();
  void OnMoveUpLanguage();

  ChromeViews::Label* languages_instructions_;
  ChromeViews::View* languages_contents_;
  ChromeViews::View* button_stack_;
  ChromeViews::TableView* language_order_table_;
  ChromeViews::NativeButton* move_up_button_;
  ChromeViews::NativeButton* move_down_button_;
  ChromeViews::NativeButton* add_button_;
  ChromeViews::NativeButton* remove_button_;
  ChromeViews::Label* language_info_label_;
  ChromeViews::Label* ui_language_label_;
  ChromeViews::ComboBox* change_ui_language_combobox_;
  ChromeViews::ComboBox* change_dictionary_language_combobox_;
  ChromeViews::Label* dictionary_language_label_;

  scoped_ptr<LanguageOrderTableModel> language_order_table_model_;
  AddLanguageView* add_language_instance_;
  StringPrefMember accept_languages_;

  // The contents of the "user interface language" combobox.
  scoped_ptr<LanguageComboboxModel> ui_language_model_;
  StringPrefMember app_locale_;

  // The contents of the "dictionary language" combobox.
  scoped_ptr<LanguageComboboxModel> dictionary_language_model_;
  StringPrefMember dictionary_language_;

  bool language_table_edited_;

  DISALLOW_EVIL_CONSTRUCTORS(LanguagesPageView);
};

#endif  // CHROME_BROWSER_VIEWS_OPTIONS_LANGUAGES_PAGE_VIEW_H__
