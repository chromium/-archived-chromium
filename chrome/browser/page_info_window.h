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

  // A counter of how many page info windows are currently opened.
  static int opened_window_count_;

  // The Window
  ChromeViews::Window* window_;

  DISALLOW_EVIL_CONSTRUCTORS(PageInfoWindow);
};

#endif  // #define CHROME_BROWSER_PAGE_INFO_WINDOW_H__