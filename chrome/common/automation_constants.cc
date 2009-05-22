// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/automation_constants.h"

namespace automation {
// JSON value labels for proxy settings that are passed in via
// AutomationMsg_SetProxyConfig.
const wchar_t kJSONProxyAutoconfig[] = L"proxy.autoconfig";
const wchar_t kJSONProxyNoProxy[] = L"proxy.no_proxy";
const wchar_t kJSONProxyPacUrl[] = L"proxy.pac_url";
const wchar_t kJSONProxyBypassList[] = L"proxy.bypass_list";
const wchar_t kJSONProxyServer[] = L"proxy.server";
}
