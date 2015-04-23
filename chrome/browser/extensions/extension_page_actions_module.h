// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_PAGE_ACTIONS_MODULE_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_PAGE_ACTIONS_MODULE_H_

#include "chrome/browser/extensions/extension_function.h"

class PageActionFunction : public SyncExtensionFunction {
 protected:
  bool SetPageActionEnabled(bool enable);
};

class EnablePageActionFunction : public PageActionFunction {
  virtual bool RunImpl();
};

class DisablePageActionFunction : public PageActionFunction {
  virtual bool RunImpl();
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_PAGE_ACTIONS_MODULE_H_
