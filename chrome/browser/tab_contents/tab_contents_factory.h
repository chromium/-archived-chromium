// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_TAB_CONTENTS_FACTORY_H_
#define CHROME_BROWSER_TAB_CONTENTS_TAB_CONTENTS_FACTORY_H_

#include <string>
#include "chrome/browser/tab_contents/tab_contents_type.h"

class GURL;
class TabContents;

// Extend from this class to implement a custom tab contents type.  See
// TabContents::RegisterFactory.
class TabContentsFactory {
 public:
  // Returns the next unused TabContentsType after TAB_CONTENTS_NUM_TYPES.
  static TabContentsType NextUnusedType();

  // Returns a new TabContents instance of the associated type.
  virtual TabContents* CreateInstance() = 0;

  // Returns true if this factory can be used to create a TabContents instance
  // capable of handling the given URL.  NOTE: the given url can be empty.
  virtual bool CanHandleURL(const GURL& url) = 0;
};

#endif  // CHROME_BROWSER_TAB_CONTENTS_TAB_CONTENTS_FACTORY_H_
