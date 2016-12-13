// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/extension_automation_constants.h"

namespace extension_automation_constants {

const char kAutomationOrigin[] = "__priv_xtapi";
const wchar_t kAutomationRequestIdKey[] = L"rqid";

const wchar_t kAutomationHasCallbackKey[] = L"hascb";
const wchar_t kAutomationErrorKey[] = L"err";
const wchar_t kAutomationNameKey[] = L"name";
const wchar_t kAutomationArgsKey[] = L"args";
const wchar_t kAutomationResponseKey[] = L"res";
const char kAutomationRequestTarget[] = "__priv_xtreq";
const char kAutomationResponseTarget[] = "__priv_xtres";

const wchar_t kAutomationConnectionIdKey[] = L"connid";
const wchar_t kAutomationMessageDataKey[] = L"data";
const wchar_t kAutomationExtensionIdKey[] = L"extid";
const wchar_t kAutomationPortIdKey[] = L"portid";
const char kAutomationPortRequestTarget[] = "__priv_prtreq";
const char kAutomationPortResponseTarget[] = "__priv_prtres";

const char kAutomationBrowserEventRequestTarget[] = "__priv_evtreq";

}  // namespace extension_automation_constants
