// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/location_bar_view_mac.h"

#include "base/sys_string_conversions.h"
#include "chrome/browser/net/url_fixer_upper.h"

LocationBarViewMac::LocationBarViewMac(NSTextField* field)
    : field_(field) {
  // TODO(shess): Placeholder for omnibox changes.
}

LocationBarViewMac::~LocationBarViewMac() {
  // TODO(shess): Placeholder for omnibox changes.
}

void LocationBarViewMac::Init() {
  // TODO(shess): Placeholder for omnibox changes.
}

std::wstring LocationBarViewMac::GetInputString() const {
  // TODO(shess): This code is temporary until the omnibox code takes
  // over.
  std::wstring url = base::SysNSStringToWide([field_ stringValue]);

  // Try to flesh out the input to make a real URL.
  return URLFixerUpper::FixupURL(url, std::wstring());
}

void LocationBarViewMac::FocusLocation() {
  [[field_ window] makeFirstResponder:field_];
}
