// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_PAGE_INFO_WINDOW_H__
#define CHROME_BROWSER_VIEWS_PAGE_INFO_WINDOW_H__

#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/views/native_button.h"
#include "chrome/views/window/dialog_delegate.h"
#include "chrome/views/window/window.h"
#include "googleurl/src/gurl.h"

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

class PageInfoWindow : public views::DialogDelegate,
                       public views::NativeButton::Listener {
 public:
  enum TabID {
    GENERAL = 0,
    SECURITY,

  };

  // Creates and shows a new page info window for the main page.
  static void CreatePageInfo(Profile* profile,
                             NavigationEntry* nav_entry,
                             HWND parent_hwnd,
                             TabID tab);

  // Creates and shows a new page info window for the frame at |url| with the
  // specified SSL information.
  static void CreateFrameInfo(Profile* profile,
                              const GURL& url,
                              const NavigationEntry::SSLStatus& ssl,
                              HWND parent_hwnd,
                              TabID tab);

  static void RegisterPrefs(PrefService* prefs);

  PageInfoWindow();
  virtual ~PageInfoWindow();

  virtual void Init(Profile* profile,
                    const GURL& url,
                    const NavigationEntry::SSLStatus& ssl,
                    NavigationEntry::PageType page_type,
                    bool show_history,
                    HWND parent);

  // views::Window overridden method.
  void Show();

  // views::NativeButton::Listener method.
  virtual void ButtonPressed(views::NativeButton* sender);

  // views::DialogDelegate methods:
  virtual int GetDialogButtons() const;
  virtual std::wstring GetWindowTitle() const;
  virtual std::wstring GetWindowName() const;
  virtual views::View* GetContentsView();

 private:
  views::View* CreateGeneralTabView();
  views::View* CreateSecurityTabView(
      Profile* profile,
      const GURL& url,
      const NavigationEntry::SSLStatus& ssl,
      NavigationEntry::PageType page_type,
      bool show_history);

  // Offsets the specified rectangle so it is showing on the screen and shifted
  // from its original location.
  void CalculateWindowBounds(CRect* bounds);

  // Shows various information for the specified certificate in a new dialog.
  void ShowCertDialog(int cert_id);

  views::NativeButton* cert_info_button_;

  // The id of the server cert for this page (0 means no cert).
  int cert_id_;

  // The page info contents.
  PageInfoContentView* contents_;

  // A counter of how many page info windows are currently opened.
  static int opened_window_count_;

  DISALLOW_EVIL_CONSTRUCTORS(PageInfoWindow);
};

#endif  // #define CHROME_BROWSER_VIEWS_PAGE_INFO_WINDOW_H__
