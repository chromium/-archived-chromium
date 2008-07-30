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
  virtual void GetPreferredSize(CSize* out);

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
