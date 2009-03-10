// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// PageNavigator defines an interface that can be used to express the user's
// intention to navigate to a particular URL.  The implementing class should
// perform the navigation.

#ifndef CHROME_BROWSER_TAB_CONTENTS_PAGE_NAVIGATOR_H_
#define CHROME_BROWSER_TAB_CONTENTS_PAGE_NAVIGATOR_H_

#include "chrome/common/page_transition_types.h"
#include "webkit/glue/window_open_disposition.h"

class GURL;

class PageNavigator {
 public:
  // Opens a URL with the given disposition.  The transition specifies how this
  // navigation should be recorded in the history system (for example, typed).
  virtual void OpenURL(const GURL& url, const GURL& referrer,
                       WindowOpenDisposition disposition,
                       PageTransition::Type transition) = 0;
};

#endif  // CHROME_BROWSER_TAB_CONTENTS_PAGE_NAVIGATOR_H_
