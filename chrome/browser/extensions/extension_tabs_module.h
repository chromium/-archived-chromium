// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_TABS_MODULE_H__
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_TABS_MODULE_H__

#include "chrome/browser/extensions/extension_function.h"

class Browser;
class TabContents;

class ExtensionTabUtil {
 public:
  static int GetWindowId(const Browser* browser);
  static int GetTabId(const TabContents* tab_contents);
  static int GetWindowIdOfTab(const TabContents* tab_contents);
};

class GetWindowsFunction : public SyncExtensionFunction {
  virtual bool RunImpl();
};
class GetTabsForWindowFunction : public SyncExtensionFunction {
  virtual bool RunImpl();
};
class CreateTabFunction : public SyncExtensionFunction {
  virtual bool RunImpl();
};
class GetTabFunction : public SyncExtensionFunction {
  virtual bool RunImpl();
};
class UpdateTabFunction : public SyncExtensionFunction {
  virtual bool RunImpl();
};
class MoveTabFunction : public SyncExtensionFunction {
  virtual bool RunImpl();
};
class RemoveTabFunction : public SyncExtensionFunction {
  virtual bool RunImpl();
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_TABS_MODULE_H__
