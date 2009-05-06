// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shell_integration.h"

#include "base/mac_util.h"
#import "third_party/mozilla/include/NSWorkspace+Utils.h"

// Sets Chromium as default browser (only for current user). Returns false if
// this operation fails (which we can't check for).
bool ShellIntegration::SetAsDefaultBrowser() {
  NSBundle* mainBundle = mac_util::MainAppBundle();
  NSString* identifier = [mainBundle bundleIdentifier];
  [[NSWorkspace sharedWorkspace] setDefaultBrowserWithIdentifier:identifier];
  return true;
}

namespace {

// Returns true if |identifier| is the bundle id of the default browser.
bool IsIdentifierDefaultBrowser(NSString* identifier) {
  NSString* defaultBrowser =
      [[NSWorkspace sharedWorkspace] defaultBrowserIdentifier];
  if (!identifier || !defaultBrowser)
    return false;
  // We need to ensure we do the comparison case-insensitive as LS doesn't
  // persist the case of our bundle id.
  NSComparisonResult result =
    [defaultBrowser caseInsensitiveCompare:identifier];
  return result == NSOrderedSame;
}

}  // namespace

// Returns true if this instance of Chromium is the default browser. (Defined
// as being the handler for the http/https protocols... we don't want to
// report false here if the user has simply chosen to open HTML files in a
// text editor and ftp links with a FTP client).
bool ShellIntegration::IsDefaultBrowser() {
  NSBundle* mainBundle = mac_util::MainAppBundle();
  NSString* myIdentifier = [mainBundle bundleIdentifier];
  return IsIdentifierDefaultBrowser(myIdentifier);
}

// Returns true if Firefox is the default browser for the current user.
bool ShellIntegration::IsFirefoxDefaultBrowser() {
  return IsIdentifierDefaultBrowser(@"org.mozilla.firefox");
}
