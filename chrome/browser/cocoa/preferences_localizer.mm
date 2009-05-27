// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/preferences_localizer.h"

#include "app/l10n_util.h"
#include "base/sys_string_conversions.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"

@implementation PreferencesLocalizer

// Override to map into our string bundles instead of strings plists.
// TODO(pinkerton): This should use a string lookup table to map the string to
// a constant.
- (NSString *)localizedStringForString:(NSString *)string {
  if ([string isEqualToString:
          @"^Make Chromium My Default Browser"]) {
    std::wstring tempString =
        l10n_util::GetStringF(IDS_OPTIONS_DEFAULTBROWSER_USEASDEFAULT,
                              l10n_util::GetString(IDS_PRODUCT_NAME));
    return base::SysWideToNSString(tempString);
  }
  return [super localizedStringForString:string];
}

@end
