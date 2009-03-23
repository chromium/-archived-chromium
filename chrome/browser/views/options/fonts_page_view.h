// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_OPTIONS_FONTS_PAGE_VIEW_H__
#define CHROME_BROWSER_VIEWS_OPTIONS_FONTS_PAGE_VIEW_H__

#include "chrome/browser/views/options/options_page_view.h"
#include "chrome/browser/shell_dialogs.h"
#include "chrome/common/pref_member.h"
#include "chrome/views/controls/combo_box.h"
#include "chrome/views/controls/button/button.h"
#include "chrome/views/view.h"


namespace views {
class GroupboxView;
class Label;
class NativeButton;
class TableModel;
class TableView;
}

class FontDisplayView;
class DefaultEncodingComboboxModel;

///////////////////////////////////////////////////////////////////////////////
// FontsPageView

class FontsPageView : public OptionsPageView,
                      public views::ComboBox::Listener,
                      public SelectFontDialog::Listener,
                      public views::ButtonListener {
 public:
  explicit FontsPageView(Profile* profile);
  virtual ~FontsPageView();

  // views::ButtonListener implementation:
  virtual void ButtonPressed(views::Button* sender);

  // views::ComboBox::Listener implementation:
  virtual void ItemChanged(views::ComboBox* combo_box,
                           int prev_index,
                           int new_index);

  // SelectFontDialog::Listener implementation:
  virtual void FontSelected(const ChromeFont& font, void* params);

  // Save Changes made to relevent pref members associated with this tab.
  // This is public since it is called by FontsLanguageWindowView in its
  // Dialog Delegate Accept() method.
  void SaveChanges();

 protected:
  // OptionsPageView implementation:
  virtual void InitControlLayout();
  virtual void NotifyPrefChanged(const std::wstring* pref_name);

 private:
  enum FontTypeBeingChanged {
    NONE,
    SERIF,
    SANS_SERIF,
    FIXED_WIDTH
  };

  // Init Dialog controls.
  void InitFontLayout();
  void InitEncodingLayout();

  bool serif_button_pressed_;
  bool sans_serif_button_pressed_;
  bool fixed_width_button_pressed_;
  bool encoding_dropdown_clicked_;

  views::Label* fonts_group_title_;
  views::Label* encoding_group_title_;

  views::View* fonts_contents_;
  views::View* encoding_contents_;

  // Fonts settings.
  // Select Font dialogs.
  scoped_refptr<SelectFontDialog> select_font_dialog_;

  // Buttons.
  views::NativeButton* fixed_width_font_change_page_button_;
  views::NativeButton* serif_font_change_page_button_;
  views::NativeButton* sans_serif_font_change_page_button_;

  // FontDisplayView objects to display selected font.
  FontDisplayView* fixed_width_font_display_view_;
  FontDisplayView* serif_font_display_view_;
  FontDisplayView* sans_serif_font_display_view_;

  // Labels to describe what is to be changed.
  views::Label* fixed_width_font_label_;
  views::Label* serif_font_label_;
  views::Label* sans_serif_font_label_;

  // Advanced Font names and sizes as PrefMembers.
  StringPrefMember serif_name_;
  StringPrefMember sans_serif_name_;
  StringPrefMember fixed_width_name_;
  IntegerPrefMember serif_size_;
  IntegerPrefMember sans_serif_size_;
  IntegerPrefMember fixed_width_size_;
  StringPrefMember default_encoding_;
  bool font_changed_;

  // Windows font picker flag;
  FontTypeBeingChanged font_type_being_changed_;

  // Default Encoding.
  scoped_ptr<DefaultEncodingComboboxModel> default_encoding_combobox_model_;
  views::Label* default_encoding_combobox_label_;
  views::ComboBox* default_encoding_combobox_;
  std::wstring default_encoding_selected_;
  bool default_encoding_changed_;

  DISALLOW_EVIL_CONSTRUCTORS(FontsPageView);
};

#endif  // #ifndef CHROME_BROWSER_VIEWS_OPTIONS_FONTS_PAGE_VIEW_H__
