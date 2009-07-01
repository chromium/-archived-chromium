// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants used for the Page Actions API.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_PAGE_ACTIONS_MODULE_CONSTANTS_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_PAGE_ACTIONS_MODULE_CONSTANTS_H_

namespace extension_page_actions_module_constants {

// Keys.
extern const wchar_t kTabIdKey[];
extern const wchar_t kUrlKey[];
extern const wchar_t kTitleKey[];
extern const wchar_t kIconIdKey[];

// Error messages.
extern const char kNoExtensionError[];
extern const char kNoTabError[];
extern const char kNoPageActionError[];
extern const char kUrlNotActiveError[];

// Function names.
extern const char kEnablePageActionFunction[];
extern const char kDisablePageActionFunction[];

};  // namespace extension_page_actions_module_constants

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_PAGE_ACTIONS_MODULE_CONSTANTS_H_
