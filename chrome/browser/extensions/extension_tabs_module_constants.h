// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants used for the Tabs API and the Windows API.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_TABS_MODULE_CONSTANTS_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_TABS_MODULE_CONSTANTS_H_

namespace extension_tabs_module_constants {

// Keys used in serializing tab data & events.
extern const wchar_t kDataKey[];
extern const wchar_t kFavIconUrlKey[];
extern const wchar_t kFocusedKey[];
extern const wchar_t kFromIndexKey[];
extern const wchar_t kHeightKey[];
extern const wchar_t kIdKey[];
extern const wchar_t kIndexKey[];
extern const wchar_t kLeftKey[];
extern const wchar_t kNewPositionKey[];
extern const wchar_t kNewWindowIdKey[];
extern const wchar_t kOldPositionKey[];
extern const wchar_t kOldWindowIdKey[];
extern const wchar_t kPageActionIdKey[];
extern const wchar_t kSelectedKey[];
extern const wchar_t kStatusKey[];
extern const wchar_t kTabIdKey[];
extern const wchar_t kTabsKey[];
extern const wchar_t kTabUrlKey[];
extern const wchar_t kTitleKey[];
extern const wchar_t kToIndexKey[];
extern const wchar_t kTopKey[];
extern const wchar_t kUrlKey[];
extern const wchar_t kWidthKey[];
extern const wchar_t kWindowIdKey[];

// Value consts.
extern const char kStatusValueComplete[];
extern const char kStatusValueLoading[];

// Error messages.
extern const char kNoCurrentWindowError[];
extern const char kNoLastFocusedWindowError[];
extern const char kWindowNotFoundError[];
extern const char kTabNotFoundError[];
extern const char kNoSelectedTabError[];
extern const char kInvalidUrlError[];

// Function names, Windows API.
extern const char kGetWindowFunction[];
extern const char kGetCurrentWindowFunction[];
extern const char kGetLastFocusedWindowFunction[];
extern const char kGetAllWindowsFunction[];
extern const char kCreateWindowFunction[];
extern const char kUpdateWindowFunction[];
extern const char kRemoveWindowFunction[];

// Function names, Tabs API.
extern const char kGetTabFunction[];
extern const char kGetSelectedTabFunction[];
extern const char kGetAllTabsInWindowFunction[];
extern const char kCreateTabFunction[];
extern const char kUpdateTabFunction[];
extern const char kMoveTabFunction[];
extern const char kRemoveTabFunction[];
extern const char kGetTabLanguageFunction[];

};  // namespace extension_tabs_module_constants

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_TABS_MODULE_CONSTANTS_H_
