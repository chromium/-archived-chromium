// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_AUTOMATION_CONSTANTS_H__
#define CHROME_COMMON_AUTOMATION_CONSTANTS_H__

namespace automation {
// JSON value labels for proxy settings that are passed in via
// AutomationMsg_SetProxyConfig. These are here since they are used by both
// AutomationProvider and AutomationProxy.
extern const wchar_t kJSONProxyAutoconfig[];
extern const wchar_t kJSONProxyNoProxy[];
extern const wchar_t kJSONProxyPacUrl[];
extern const wchar_t kJSONProxyBypassList[];
extern const wchar_t kJSONProxyServer[];
}

#endif  // CHROME_COMMON_AUTOMATION_CONSTANTS_H__
