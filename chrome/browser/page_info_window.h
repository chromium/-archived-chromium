// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_INFO_WINDOW_H__
#define CHROME_BROWSER_PAGE_INFO_WINDOW_H__

#include "base/gfx/native_widget_types.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "googleurl/src/gurl.h"
#include "net/base/x509_certificate.h"

// The page info window displays information regarding the current page,
// including security information.

class NavigationEntry;
class PageInfoContentView;
class PrefService;
class Profile;

class PageInfoWindow {
 public:
  enum TabID {
    GENERAL = 0,
    SECURITY,
  };

  // Factory method to get a new platform impl of PageInfoWindow
  static PageInfoWindow* Factory();

  // Creates and shows a new page info window for the main page.
  static void CreatePageInfo(Profile* profile,
                             NavigationEntry* nav_entry,
                             gfx::NativeView parent,
                             TabID tab);

  // Creates and shows a new page info window for the frame at |url| with the
  // specified SSL information.
  static void CreateFrameInfo(Profile* profile,
                              const GURL& url,
                              const NavigationEntry::SSLStatus& ssl,
                              gfx::NativeView parent,
                              TabID tab);

  static void RegisterPrefs(PrefService* prefs);

  PageInfoWindow();
  virtual ~PageInfoWindow();

  // This is the main initializer that creates the window.
  virtual void Init(Profile* profile,
                    const GURL& url,
                    const NavigationEntry::SSLStatus& ssl,
                    NavigationEntry::PageType page_type,
                    bool show_history,
                    gfx::NativeView parent) = 0;

  // Brings the page info window to the foreground.
  virtual void Show() = 0;

  // Shows various information for the specified certificate in a new dialog.
  // This can be implemented as an individual window (like on Windows), or as
  // a modal dialog/sheet (on Mac). Either will work since we're only expecting
  // one certificate per page.
  virtual void ShowCertDialog(int cert_id) = 0;

 protected:
  // Returns a name that can be used to represent the issuer.  It tries in this
  // order CN, O and OU and returns the first non-empty one found.
  static std::string GetIssuerName(
      const net::X509Certificate::Principal& issuer);

  // The id of the server cert for this page (0 means no cert).
  int cert_id_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PageInfoWindow);
};

#endif  // #define CHROME_BROWSER_PAGE_INFO_WINDOW_H__
