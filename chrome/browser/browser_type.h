// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_TYPE_H__
#define CHROME_BROWSER_BROWSER_TYPE_H__

#include "base/basictypes.h"

class BrowserType {
 public:
  // Enumeration of the types of Browsers we have. This is defined outside
  // of Browser to avoid cyclical dependencies.
  enum Type {
    // NOTE: If you change this list, you may need to update which browsers
    // are saved in session history. See
    // SessionService::BuildCommandsFromBrowser.
    TABBED_BROWSER = 0,   // A normal chrome window.
    BROWSER,              // A chrome window without the tabstrip or toolbar.
    APPLICATION,          // Web application with a frame but no tab strips.
  };

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(BrowserType);
};

#endif  // CHROME_BROWSER_BROWSER_TYPE_H__

