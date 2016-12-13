// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_page_actions_module_constants.h"

namespace extension_page_actions_module_constants {

const wchar_t kTabIdKey[] = L"tabId";
const wchar_t kUrlKey[] = L"url";
const wchar_t kTitleKey[] = L"title";
const wchar_t kIconIdKey[] = L"iconId";

const char kNoExtensionError[] = "No extension with id: *.";
const char kNoTabError[] = "No tab with id: *.";
const char kNoPageActionError[] = "No PageAction with id: *.";
const char kUrlNotActiveError[] = "This url is no longer active: *.";

const char kEnablePageActionFunction[] = "EnablePageAction";
const char kDisablePageActionFunction[] = "DisablePageAction";

}  // namespace extension_page_actions_module_constants
