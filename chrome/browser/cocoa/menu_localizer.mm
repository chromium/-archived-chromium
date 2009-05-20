// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/menu_localizer.h"

#include "app/l10n_util.h"
#include "base/sys_string_conversions.h"
#include "grit/chromium_strings.h"

@implementation MenuLocalizer

// Override to map into our string bundles instead of strings plists.
// TODO(pinkerton): This should use a string lookup table to map the string to
// a constant.
- (NSString *)localizedStringForString:(NSString *)string {
  if ([string isEqualToString:@"^Quit Chromium"]) {
    std::wstring quitString = l10n_util::GetString(IDS_EXIT_MAC);
    return base::SysWideToNSString(quitString);
  } else if ([string isEqualToString:@"^About Chromium"]) {
    std::wstring quitString = l10n_util::GetString(IDS_ABOUT_CHROME_TITLE);
    return base::SysWideToNSString(quitString);
  } else if ([string isEqualToString:@"^Hide Chromium"]) {
    std::wstring quitString = l10n_util::GetString(IDS_HIDE_MAC);
    return base::SysWideToNSString(quitString);
  } else if ([string isEqualToString:@"^Chromium Help"]) {
    std::wstring quitString = l10n_util::GetString(IDS_HELP_MAC);
    return base::SysWideToNSString(quitString);
  }
  return [super localizedStringForString:string];
}

@end
