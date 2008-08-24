// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHELL_INTEGRATION_H__
#define CHROME_BROWSER_SHELL_INTEGRATION_H__

#include <string>

class ShellIntegration {
 public:
  // Makes sure we are installed as a handler for the types we care about.
  // It should be called once, probably at startup.
  //
  // It will not check everything right away, but will instead do it on a timer
  // to avoid blocking startup.
  static void VerifyInstallation();

  // Like VerifyInstallation() but does the operations synchronously, returning
  // true on success.
  static bool VerifyInstallationNow();

  // Sets Chrome as default browser (only for current user). Returns false if
  // this operation fails.
  static bool SetAsDefaultBrowser();

  // Returns true if this instance of Chrome is the default browser. (Defined
  // as being the handler for the http/https protocols... we don't want to
  // report false here if the user has simply chosen to open HTML files in a
  // text editor and ftp links with a FTP client).
  static bool IsDefaultBrowser();

  // Returns true if IE is likely to be the default browser for the current
  // user. This method is very fast so it can be invoked in the UI thread.
  static bool IsFirefoxDefaultBrowser();
};

#endif  // CHROME_BROWSER_SHELL_INTEGRATION_H__

