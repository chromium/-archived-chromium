// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants used to encode requests and responses for automation.

#ifndef CHROME_TEST_AUTOMATION_EXTENSION_AUTOMATION_CONSTANTS_H_
#define CHROME_TEST_AUTOMATION_EXTENSION_AUTOMATION_CONSTANTS_H_

namespace extension_automation_constants {

extern const wchar_t kAutomationRequestIdKey[];
extern const wchar_t kAutomationHasCallbackKey[];
extern const wchar_t kAutomationErrorKey[];  // not present implies success
extern const wchar_t kAutomationNameKey[];
extern const wchar_t kAutomationArgsKey[];
extern const wchar_t kAutomationResponseKey[];
extern const char kAutomationOrigin[];
extern const char kAutomationRequestTarget[];
extern const char kAutomationResponseTarget[];

};  // namespace automation_extension_constants

#endif  // CHROME_TEST_AUTOMATION_EXTENSION_AUTOMATION_CONSTANTS_H_
