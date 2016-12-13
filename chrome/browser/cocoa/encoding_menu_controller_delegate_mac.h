// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_ENCODING_MENU_CONTROLLER_DELEGATE_MAC_H_
#define CHROME_BROWSER_COCOA_ENCODING_MENU_CONTROLLER_DELEGATE_MAC_H_

#include "base/basictypes.h"  // For DISALLOW_IMPLICIT_CONSTRUCTORS

class Profile;

// The Windows version of this class manages the Encoding Menu, but since Cocoa
// does that for us automagically, the only thing left to do is construct
// the encoding menu.
class EncodingMenuControllerDelegate {
 public:
  static void BuildEncodingMenu(Profile *profile);
 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(EncodingMenuControllerDelegate);
};

#endif  // CHROME_BROWSER_COCOA_ENCODING_MENU_CONTROLLER_DELEGATE_MAC_H_
