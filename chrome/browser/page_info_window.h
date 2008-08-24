// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_INFO_WINDOW_H__
#define CHROME_BROWSER_PAGE_INFO_WINDOW_H__

#include "chrome/views/dialog_delegate.h"
#include "chrome/views/native_button.h"
#include "chrome/views/window.h"

// The page info window displays information regarding the current page,
// including security information.

namespace ChromeViews {
class TabbedPane;
}

class NavigationEntry;
class PageInfoContentView;
class PrefService;
class Profile;
class X509Certificate;

class PageInfoWindow : public ChromeViews::DialogDelegate,
                       public ChromeViews::NativeButton::Listener {
 public:
  enum TabID {
    GENERAL = 0,
    SECURITY,

  };

  // Creates and shows a new PageInfoWindow.
  static void Create(Profile* profile,
                     NavigationEntry* nav_entry,
                     HWND parent_hwnd,
                     TabID tab);

  static void RegisterPrefs(PrefService* prefs);

  PageInfoWindow();
  virtual ~PageInfoWindow();

  virtual void Init(Profile* profile,
                    NavigationEntry* navigation_entry,
                    HWND parent);

  // ChromeViews::Window overridden method.
  void Show();

  // ChromeViews::NativeButton::Listener method.
  virtual void ButtonPressed(ChromeViews::NativeButton* sender);

  // ChromeViews::DialogDelegate methods:
  virtual int GetDialogButtons() const;
  virtual std::wstring GetWindowTitle() const;
  virtual void SaveWindowPosition(const CRect& bounds,
                                  bool maximized,
                                  bool always_on_top);
  virtual bool RestoreWindowPosition(CRect* bounds,
                                     bool* maximized,
                                     bool* always_on_top);
  virtual ChromeViews::View* GetContentsView();

 private:
  ChromeViews::View* CreateGeneralTabView();
  ChromeViews::View* CreateSecurityTabView(Profile* profile,
                                           NavigationEntry* navigation_entry);

  // Offsets the specified rectangle so it is showing on the screen and shifted
  // from its original location.
  void CalculateWindowBounds(CRect* bounds);

  // Shows various information for the specified certificate in a new dialog.
  void ShowCertDialog(int cert_id);

  ChromeViews::NativeButton* cert_info_button_;

  // The id of the server cert for this page (0 means no cert).
  int cert_id_;

  // The page info contents.
  PageInfoContentView* contents_;

  // A counter of how many page info windows are currently opened.
  static int opened_window_count_;

  DISALLOW_EVIL_CONSTRUCTORS(PageInfoWindow);
};

#endif  // #define CHROME_BROWSER_PAGE_INFO_WINDOW_H__
