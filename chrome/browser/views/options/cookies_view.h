// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_OPTIONS_COOKIES_VIEW_H__
#define CHROME_BROWSER_VIEWS_OPTIONS_COOKIES_VIEW_H__

#include "base/task.h"
#include "chrome/views/native_button.h"
#include "chrome/views/table_view.h"
#include "chrome/views/text_field.h"
#include "chrome/views/view.h"
#include "chrome/views/window/dialog_delegate.h"
#include "chrome/views/window/window.h"

namespace views {
class Label;
}
class CookieInfoView;
class CookiesTableModel;
class CookiesTableView;
class Profile;
class Timer;

class CookiesView : public views::View,
                    public views::DialogDelegate,
                    public views::NativeButton::Listener,
                    public views::TableViewObserver,
                    public views::TextField::Controller {
 public:
  // Show the Cookies Window, creating one if necessary.
  static void ShowCookiesWindow(Profile* profile);

  virtual ~CookiesView();

  // Updates the display to show only the search results.
  void UpdateSearchResults();

  // views::NativeButton::Listener implementation:
  virtual void ButtonPressed(views::NativeButton* sender);

  // views::TableViewObserver implementation:
  virtual void OnSelectionChanged();

  // Invoked when the user presses the delete key. Deletes the selected
  // cookies.
  virtual void OnTableViewDelete(views::TableView* table_view);

  // views::TextField::Controller implementation:
  virtual void ContentsChanged(views::TextField* sender,
                               const std::wstring& new_contents);
  virtual void HandleKeystroke(views::TextField* sender,
                               UINT message, TCHAR key, UINT repeat_count,
                               UINT flags);

  // views::WindowDelegate implementation:
  virtual int GetDialogButtons() const { return DIALOGBUTTON_CANCEL; }
  virtual views::View* GetInitiallyFocusedView() {
      return search_field_;
  }
  virtual bool CanResize() const { return true; }
  virtual std::wstring GetWindowTitle() const;
  virtual void WindowClosing();
  virtual views::View* GetContentsView();

  // views::View overrides:
  virtual void Layout();
  virtual gfx::Size GetPreferredSize();

 protected:
  // views::View overrides:
  virtual void ViewHierarchyChanged(bool is_add,
                                    views::View* parent,
                                    views::View* child);

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
  views::Label* search_label_;
  views::TextField* search_field_;
  views::NativeButton* clear_search_button_;
  views::Label* description_label_;
  CookiesTableView* cookies_table_;
  CookieInfoView* info_view_;
  views::NativeButton* remove_button_;
  views::NativeButton* remove_all_button_;

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
  static views::Window* instance_;

  DISALLOW_EVIL_CONSTRUCTORS(CookiesView);
};

#endif  // #ifndef CHROME_BROWSER_VIEWS_OPTIONS_GENERAL_PAGE_VIEW_H__
