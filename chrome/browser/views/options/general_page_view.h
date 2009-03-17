// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_OPTIONS_GENERAL_PAGE_VIEW_H_
#define CHROME_BROWSER_VIEWS_OPTIONS_GENERAL_PAGE_VIEW_H_

#include "chrome/browser/views/options/options_page_view.h"
#include "chrome/browser/views/shelf_item_dialog.h"
#include "chrome/common/pref_member.h"
#include "chrome/views/controls/combo_box.h"
#include "chrome/views/controls/button/native_button.h"
#include "chrome/views/view.h"

namespace views {
class CheckBox;
class GroupboxView;
class Label;
class RadioButton;
class TableModel;
class TableView;
class TextField;
}
class CustomHomePagesTableModel;
class OptionsGroupView;
class SearchEngineListModel;

///////////////////////////////////////////////////////////////////////////////
// GeneralPageView

class GeneralPageView : public OptionsPageView,
                        public views::ComboBox::Listener,
                        public views::NativeButton::Listener,
                        public views::TextField::Controller,
                        public ShelfItemDialogDelegate,
                        public views::TableViewObserver {
 public:
  explicit GeneralPageView(Profile* profile);
  virtual ~GeneralPageView();

 protected:
  // views::NativeButton::Listener implementation:
  virtual void ButtonPressed(views::NativeButton* sender);

  // views::ComboBox::Listener implementation:
  virtual void ItemChanged(views::ComboBox* combo_box,
                           int prev_index,
                           int new_index);

  // views::TextField::Controller implementation:
  virtual void ContentsChanged(views::TextField* sender,
     const std::wstring& new_contents);
  virtual void HandleKeystroke(views::TextField* sender,
     UINT message, TCHAR key, UINT repeat_count,
     UINT flags);

  // OptionsPageView implementation:
  virtual void InitControlLayout();
  virtual void NotifyPrefChanged(const std::wstring* pref_name);
  virtual void HighlightGroup(OptionsGroup highlight_group);

  // views::View overrides:
  virtual void Layout();

 private:
  // The current default browser UI state
  enum DefaultBrowserUIState {
    STATE_PROCESSING,
    STATE_DEFAULT,
    STATE_NOT_DEFAULT
  };
  // Updates the UI state to reflect the current default browser state.
  void SetDefaultBrowserUIState(DefaultBrowserUIState state);

  // Init all the dialog controls
  void InitStartupGroup();
  void InitHomepageGroup();
  void InitDefaultSearchGroup();
  void InitDefaultBrowserGroup();

  // Saves the startup preference from that of the ui.
  void SaveStartupPref();

  // Shows a dialog allowing the user to add a new URL to the set of URLs
  // launched on startup.
  void AddURLToStartupURLs();

  // Removes the selected URL from the list of startup urls.
  void RemoveURLsFromStartupURLs();

  // Resets the list of urls to launch on startup from the list of open
  // browsers.
  void SetStartupURLToCurrentPage();

  // Enables/Disables the controls associated with the custom start pages
  // option if that preference is not selected.
  void EnableCustomHomepagesControls(bool enable);

  // ShelfItemDialogDelegate. Adds the URL to the list of startup urls.
  virtual void AddBookmark(ShelfItemDialog* dialog,
                           const std::wstring& title,
                           const GURL& url);

  // Sets the home page preferences for kNewTabPageIsHomePage and kHomePage.
  // If a blank string is passed in we revert to using NewTab page as the Home
  // page. When setting the Home Page to NewTab page, we preserve the old value
  // of kHomePage (we don't overwrite it).
  void SetHomepage(const std::wstring& homepage);

  // Invoked when the selection of the table view changes. Updates the enabled
  // property of the remove button.
  virtual void OnSelectionChanged();

  // Enables or disables the field for entering a custom homepage URL.
  void EnableHomepageURLField(bool enabled);

  // Sets the default search provider for the selected item in the combobox.
  void SetDefaultSearchProvider();

  // Controls for the Startup group
  OptionsGroupView* startup_group_;
  views::RadioButton* startup_homepage_radio_;
  views::RadioButton* startup_last_session_radio_;
  views::RadioButton* startup_custom_radio_;
  views::NativeButton* startup_add_custom_page_button_;
  views::NativeButton* startup_remove_custom_page_button_;
  views::NativeButton* startup_use_current_page_button_;
  views::TableView* startup_custom_pages_table_;
  scoped_ptr<CustomHomePagesTableModel> startup_custom_pages_table_model_;

  // Controls for the Home Page group
  OptionsGroupView* homepage_group_;
  views::RadioButton* homepage_use_newtab_radio_;
  views::RadioButton* homepage_use_url_radio_;
  views::TextField* homepage_use_url_textfield_;
  views::CheckBox* homepage_show_home_button_checkbox_;
  BooleanPrefMember new_tab_page_is_home_page_;
  StringPrefMember homepage_;
  BooleanPrefMember show_home_button_;

  // Controls for the Default Search group
  OptionsGroupView* default_search_group_;
  views::ComboBox* default_search_engine_combobox_;
  views::NativeButton* default_search_manage_engines_button_;
  scoped_ptr<SearchEngineListModel> default_search_engines_model_;

  // Controls for the Default Browser group
  OptionsGroupView* default_browser_group_;
  views::Label* default_browser_status_label_;
  views::NativeButton* default_browser_use_as_default_button_;

  // The helper object that performs default browser set/check tasks.
  class DefaultBrowserWorker;
  friend DefaultBrowserWorker;
  scoped_refptr<DefaultBrowserWorker> default_browser_worker_;

  DISALLOW_EVIL_CONSTRUCTORS(GeneralPageView);
};

#endif  // CHROME_BROWSER_VIEWS_OPTIONS_GENERAL_PAGE_VIEW_H_
