// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_OPTIONS_COOKIES_VIEW_H__
#define CHROME_BROWSER_VIEWS_OPTIONS_COOKIES_VIEW_H__

#include "chrome/views/dialog_delegate.h"
#include "chrome/views/native_button.h"
#include "chrome/views/table_view.h"
#include "chrome/views/text_field.h"
#include "chrome/views/view.h"
#include "chrome/views/window.h"

namespace ChromeViews {
class Label;
}
class CookieInfoView;
class CookiesTableModel;
class CookiesTableView;
class Profile;
class Timer;

class CookiesView : public ChromeViews::View,
                    public ChromeViews::DialogDelegate,
                    public ChromeViews::NativeButton::Listener,
                    public ChromeViews::TableViewObserver,
                    public ChromeViews::TextField::Controller {
 public:
  // Show the Cookies Window, creating one if necessary.
  static void ShowCookiesWindow(Profile* profile);

  virtual ~CookiesView();

  // Updates the display to show only the search results.
  void UpdateSearchResults();

  // ChromeViews::NativeButton::Listener implementation:
  virtual void ButtonPressed(ChromeViews::NativeButton* sender);

  // ChromeViews::TableViewObserver implementation:
  virtual void OnSelectionChanged();

  // ChromeViews::TextField::Controller implementation:
  virtual void ContentsChanged(ChromeViews::TextField* sender,
                               const std::wstring& new_contents);
  virtual void HandleKeystroke(ChromeViews::TextField* sender,
                               UINT message, TCHAR key, UINT repeat_count,
                               UINT flags);

  // ChromeViews::WindowDelegate implementation:
  virtual int GetDialogButtons() const { return DIALOGBUTTON_CANCEL; }
  virtual ChromeViews::View* GetInitiallyFocusedView() const {
      return search_field_;
  }
  virtual bool CanResize() const { return true; }
  virtual std::wstring GetWindowTitle() const;
  virtual void WindowClosing();
  virtual ChromeViews::View* GetContentsView();

  // ChromeViews::View overrides:
  virtual void Layout();
  virtual gfx::Size GetPreferredSize();

 protected:
  // ChromeViews::View overrides:
  virtual void ViewHierarchyChanged(bool is_add,
                                    ChromeViews::View* parent,
                                    ChromeViews::View* child);

 private:
  // Use the static factory method to show.
  explicit CookiesView(Profile* profile);

  // Initialize the dialog contents and layout.
  void Init();

  // Resets the display to what it would be if there were no search query.
  void ResetSearchQuery();

  // Update the UI when there are no cookies.
  void UpdateForEmptyState();

  // Assorted dialog controls
  ChromeViews::Label* search_label_;
  ChromeViews::TextField* search_field_;
  ChromeViews::NativeButton* clear_search_button_;
  ChromeViews::Label* description_label_;
  CookiesTableView* cookies_table_;
  CookieInfoView* info_view_;
  ChromeViews::NativeButton* remove_button_;
  ChromeViews::NativeButton* remove_all_button_;

  // The Cookies Table model
  scoped_ptr<CookiesTableModel> cookies_table_model_;
  scoped_ptr<CookiesTableModel> search_table_model_;

  // The Profile for which Cookies are displayed
  Profile* profile_;

  // A factory to construct Runnable Methods so that we can be called back to
  // re-evaluate the model after the search query string changes.
  ScopedRunnableMethodFactory<CookiesView> search_update_factory_;

  // Our containing window. If this is non-NULL there is a visible Cookies
  // window somewhere.
  static ChromeViews::Window* instance_;

  DISALLOW_EVIL_CONSTRUCTORS(CookiesView);
};

#endif  // #ifndef CHROME_BROWSER_VIEWS_OPTIONS_GENERAL_PAGE_VIEW_H__

