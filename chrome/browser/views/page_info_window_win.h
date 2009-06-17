// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_PAGE_INFO_WINDOW_WIN_H__
#define CHROME_BROWSER_VIEWS_PAGE_INFO_WINDOW_WIN_H__

#include "chrome/browser/page_info_window.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "googleurl/src/gurl.h"
#include "views/controls/button/button.h"
#include "views/window/dialog_delegate.h"
#include "views/window/window.h"

// The page info window displays information regarding the current page,
// including security information.

namespace views {
class TabbedPane;
}

class NavigationEntry;
class PageInfoContentView;
class PrefService;
class Profile;
class X509Certificate;
class SecurityTabView;

class PageInfoWindowWin : public PageInfoWindow,
                          public views::DialogDelegate,
                          public views::ButtonListener {
 public:
  // We need access to PageInfoWindow::GetIssuerName() which is protected
  friend class SecurityTabView;

  PageInfoWindowWin();
  virtual ~PageInfoWindowWin();

  // This is the main initializer that creates the window.
  virtual void Init(Profile* profile,
                    const GURL& url,
                    const NavigationEntry::SSLStatus& ssl,
                    NavigationEntry::PageType page_type,
                    bool show_history,
                    gfx::NativeView parent);

  // views::Window overridden method.
  virtual void Show();
  virtual void ShowCertDialog(int cert_id);

  // views::ButtonListener method.
  virtual void ButtonPressed(views::Button* sender);

  // views::DialogDelegate methods:
  virtual int GetDialogButtons() const;
  virtual std::wstring GetWindowTitle() const;
  virtual std::wstring GetWindowName() const;
  virtual views::View* GetContentsView();

 private:
  virtual views::View* CreateGeneralTabView();
  virtual views::View* CreateSecurityTabView(
      Profile* profile,
      const GURL& url,
      const NavigationEntry::SSLStatus& ssl,
      NavigationEntry::PageType page_type,
      bool show_history);

  // Offsets the specified rectangle so it is showing on the screen and shifted
  // from its original location.
  void CalculateWindowBounds(gfx::Rect* bounds);

  views::NativeButton* cert_info_button_;

  // The page info contents.
  PageInfoContentView* contents_;

  // A counter of how many page info windows are currently opened.
  static int opened_window_count_;

  DISALLOW_COPY_AND_ASSIGN(PageInfoWindowWin);
};

#endif  // #define CHROME_BROWSER_VIEWS_PAGE_INFO_WINDOW_WIN_H__
