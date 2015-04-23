// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_tabs_module_constants.h"

namespace extension_tabs_module_constants {

const wchar_t kDataKey[] = L"data";
const wchar_t kFavIconUrlKey[] = L"favIconUrl";
const wchar_t kFocusedKey[] = L"focused";
const wchar_t kFromIndexKey[] = L"fromIndex";
const wchar_t kHeightKey[] = L"height";
const wchar_t kIdKey[] = L"id";
const wchar_t kIndexKey[] = L"index";
const wchar_t kLeftKey[] = L"left";
const wchar_t kNewPositionKey[] = L"newPosition";
const wchar_t kNewWindowIdKey[] = L"newWindowId";
const wchar_t kOldPositionKey[] = L"oldPosition";
const wchar_t kOldWindowIdKey[] = L"oldWindowId";
const wchar_t kPageActionIdKey[] = L"pageActionId";
const wchar_t kSelectedKey[] = L"selected";
const wchar_t kStatusKey[] = L"status";
const wchar_t kTabIdKey[] = L"tabId";
const wchar_t kTabsKey[] = L"tabs";
const wchar_t kTabUrlKey[] = L"tabUrl";
const wchar_t kTitleKey[] = L"title";
const wchar_t kToIndexKey[] = L"toIndex";
const wchar_t kTopKey[] = L"top";
const wchar_t kUrlKey[] = L"url";
const wchar_t kWidthKey[] = L"width";
const wchar_t kWindowIdKey[] = L"windowId";

const char kStatusValueComplete[] = "complete";
const char kStatusValueLoading[] = "loading";

const char kNoCurrentWindowError[] = "No current window";
const char kNoLastFocusedWindowError[] = "No last-focused window";
const char kWindowNotFoundError[] = "No window with id: *.";
const char kTabNotFoundError[] = "No tab with id: *.";
const char kNoSelectedTabError[] = "No selected tab";
const char kInvalidUrlError[] = "Invalid url: \"*\".";

const char kGetWindowFunction[] = "GetWindow";
const char kGetCurrentWindowFunction[] = "GetCurrentWindow";
const char kGetLastFocusedWindowFunction[] = "GetLastFocusedWindow";
const char kGetAllWindowsFunction[] = "GetAllWindows";
const char kCreateWindowFunction[] = "CreateWindow";
const char kUpdateWindowFunction[] = "UpdateWindow";
const char kRemoveWindowFunction[] = "RemoveWindow";

const char kGetTabFunction[] = "GetTab";
const char kGetSelectedTabFunction[] = "GetSelectedTab";
const char kGetAllTabsInWindowFunction[] = "GetAllTabsInWindow";
const char kCreateTabFunction[] = "CreateTab";
const char kUpdateTabFunction[] = "UpdateTab";
const char kMoveTabFunction[] = "MoveTab";
const char kRemoveTabFunction[] = "RemoveTab";
const char kGetTabLanguageFunction[] = "GetTabLanguage";

}  // namespace extension_tabs_module_constants
