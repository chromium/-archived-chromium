// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_OPTIONS_LANGUAGES_PAGE_VIEW_H__
#define CHROME_BROWSER_VIEWS_OPTIONS_LANGUAGES_PAGE_VIEW_H__

#include "chrome/browser/views/options/options_page_view.h"
#include "chrome/common/pref_member.h"
#include "chrome/views/controls/combo_box.h"
#include "chrome/views/controls/button/native_button.h"
#include "chrome/views/controls/table/table_view.h"
#include "chrome/views/view.h"

namespace views {
class CheckBox;
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
                          public views::NativeButton::Listener,
                          public views::TableViewObserver,
                          public views::ComboBox::Listener {
 public:
  explicit LanguagesPageView(Profile* profile);
  virtual ~LanguagesPageView();

  // views::NativeButton::Listener implementation:
  virtual void ButtonPressed(views::NativeButton* sender);

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

  // views::ComboBox::Listener implementation:
  virtual void ItemChanged(views::ComboBox* sender,
                           int prev_index,
                           int new_index);

 private:
  // Invoked when the selection of the table view changes. Updates the enabled
  // property of the remove button.
  virtual void OnSelectionChanged();
  void OnRemoveLanguage();
  void OnMoveDownLanguage();
  void OnMoveUpLanguage();

  views::Label* languages_instructions_;
  views::View* languages_contents_;
  views::View* button_stack_;
  views::TableView* language_order_table_;
  views::NativeButton* move_up_button_;
  views::NativeButton* move_down_button_;
  views::NativeButton* add_button_;
  views::NativeButton* remove_button_;
  views::Label* language_info_label_;
  views::Label* ui_language_label_;
  views::ComboBox* change_ui_language_combobox_;
  views::ComboBox* change_dictionary_language_combobox_;
  views::CheckBox* enable_spellchecking_checkbox_;
  views::Label* dictionary_language_label_;

  scoped_ptr<LanguageOrderTableModel> language_order_table_model_;
  AddLanguageView* add_language_instance_;
  StringPrefMember accept_languages_;

  // The contents of the "user interface language" combobox.
  scoped_ptr<LanguageComboboxModel> ui_language_model_;
  StringPrefMember app_locale_;
  int ui_language_index_selected_;
  int starting_ui_language_index_;

  // The contents of the "dictionary language" combobox.
  scoped_ptr<LanguageComboboxModel> dictionary_language_model_;
  StringPrefMember dictionary_language_;

  // SpellChecker enable pref.
  BooleanPrefMember enable_spellcheck_;

  // This is assigned the new index of spellcheck language if the language
  // is changed. Otherwise, it remains -1, and pref members are not updated.
  int spellcheck_language_index_selected_;
  std::wstring spellcheck_language_added_;

  bool language_table_edited_;
  bool language_warning_shown_;

  DISALLOW_EVIL_CONSTRUCTORS(LanguagesPageView);
};

#endif  // CHROME_BROWSER_VIEWS_OPTIONS_LANGUAGES_PAGE_VIEW_H__
