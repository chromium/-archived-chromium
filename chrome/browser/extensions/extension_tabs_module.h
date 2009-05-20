// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_TABS_MODULE_H__
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_TABS_MODULE_H__

#include <string>

#include "chrome/browser/extensions/extension_function.h"

class Browser;
class DictionaryValue;
class TabContents;
class TabStripModel;

class ExtensionTabUtil {
 public:
  // Possible tab states.  These states are used to calculate the "status"
  // property of the Tab object that is used in the extension tab API.
  enum TabStatus {
    TAB_LOADING,  // Waiting for the DOM to load.
    TAB_COMPLETE  // Tab loading and rendering is complete.
  };

  // Keys used in serializing tab data & events.
  static const wchar_t* kDataKey;
  static const wchar_t* kFavIconUrlKey;
  static const wchar_t* kFocusedKey;
  static const wchar_t* kFromIndexKey;
  static const wchar_t* kHeightKey;
  static const wchar_t* kIdKey;
  static const wchar_t* kIndexKey;
  static const wchar_t* kLeftKey;
  static const wchar_t* kNewPositionKey;
  static const wchar_t* kNewWindowIdKey;
  static const wchar_t* kOldPositionKey;
  static const wchar_t* kOldWindowIdKey;
  static const wchar_t* kPageActionIdKey;
  static const wchar_t* kSelectedKey;
  static const wchar_t* kStatusKey;
  static const wchar_t* kTabIdKey;
  static const wchar_t* kTabsKey;
  static const wchar_t* kTabUrlKey;
  static const wchar_t* kTitleKey;
  static const wchar_t* kToIndexKey;
  static const wchar_t* kTopKey;
  static const wchar_t* kUrlKey;
  static const wchar_t* kWidthKey;
  static const wchar_t* kWindowIdKey;

  // Value consts.
  static const char* kStatusValueComplete;
  static const char* kStatusValueLoading;

  static int GetWindowId(const Browser* browser);
  static int GetTabId(const TabContents* tab_contents);
  static TabStatus GetTabStatus(const TabContents* tab_contents);
  static std::string GetTabStatusText(TabStatus status);
  static int GetWindowIdOfTab(const TabContents* tab_contents);
  static DictionaryValue* CreateTabValue(const TabContents* tab_contents);
  static DictionaryValue* CreateTabValue(const TabContents* tab_contents,
                                         TabStripModel* tab_strip,
                                         int tab_index);

  // Any out parameter (|browser|, |tab_strip|, |contents|, & |tab_index|) may
  // be NULL and will not be set within the function.
  static bool GetTabById(int tab_id, Profile* profile, Browser** browser,
                         TabStripModel** tab_strip,
                         TabContents** contents,
                         int* tab_index);
};

// Windows
class GetWindowFunction : public SyncExtensionFunction {
  virtual bool RunImpl();
};
class GetCurrentWindowFunction : public SyncExtensionFunction {
  virtual bool RunImpl();
};
class GetLastFocusedWindowFunction : public SyncExtensionFunction {
  virtual bool RunImpl();
};
class GetAllWindowsFunction : public SyncExtensionFunction {
  virtual bool RunImpl();
};
class CreateWindowFunction : public SyncExtensionFunction {
  virtual bool RunImpl();
};
class UpdateWindowFunction : public SyncExtensionFunction {
  virtual bool RunImpl();
};
class RemoveWindowFunction : public SyncExtensionFunction {
  virtual bool RunImpl();
};

// Tabs
class GetTabFunction : public SyncExtensionFunction {
  virtual bool RunImpl();
};
class GetSelectedTabFunction : public SyncExtensionFunction {
  virtual bool RunImpl();
};
class GetAllTabsInWindowFunction : public SyncExtensionFunction {
  virtual bool RunImpl();
};
class CreateTabFunction : public SyncExtensionFunction {
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
