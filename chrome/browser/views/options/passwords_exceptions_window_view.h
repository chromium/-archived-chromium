// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_OPTIONS_PASSWORDS_EXCEPTIONS_WINDOW_VIEW_H_
#define CHROME_BROWSER_VIEWS_OPTIONS_PASSWORDS_EXCEPTIONS_WINDOW_VIEW_H_

#include "views/controls/tabbed_pane.h"
#include "views/view.h"
#include "views/window/dialog_delegate.h"
#include "views/window/window.h"

class Profile;
class PasswordsPageView;
class ExceptionsPageView;

///////////////////////////////////////////////////////////////////////////////
// PasswordsExceptionsWindowView
//
//  The contents of the "Save passwords and exceptions" dialog window.
//
class PasswordsExceptionsWindowView : public views::View,
                                      public views::DialogDelegate {
 public:
  explicit PasswordsExceptionsWindowView(Profile* profile);
  virtual ~PasswordsExceptionsWindowView() {}

  // Show the PasswordManagerExceptionsView for the given profile.
  static void Show(Profile* profile);

  // views::View methods.
  virtual void Layout();
  virtual gfx::Size GetPreferredSize();
  virtual void ViewHierarchyChanged(bool is_add, views::View* parent,
                                    views::View* child);

  // views::DialogDelegate methods:
  virtual int GetDialogButtons() const;
  virtual bool CanResize() const { return true; }
  virtual bool CanMaximize() const { return false; }
  virtual bool IsAlwaysOnTop() const { return false; }
  virtual bool HasAlwaysOnTopMenu() const { return false; }
  virtual std::wstring GetWindowTitle() const;
  virtual void WindowClosing();
  virtual views::View* GetContentsView();

 private:
  void Init();

  // The Tab view that contains all of the options pages.
  views::TabbedPane* tabs_;

  PasswordsPageView* passwords_page_view_;

  ExceptionsPageView* exceptions_page_view_;

  Profile* profile_;

  static PasswordsExceptionsWindowView* instance_;

  DISALLOW_COPY_AND_ASSIGN(PasswordsExceptionsWindowView);
};

#endif  // CHROME_BROWSER_VIEWS_OPTIONS_PASSWORDS_EXCEPTIONS_WINDOW_VIEW_H_
