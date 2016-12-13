// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants used to encode requests and responses for automation.

#ifndef CHROME_BROWSER_AUTOMATION_EXTENSION_AUTOMATION_CONSTANTS_H_
#define CHROME_BROWSER_AUTOMATION_EXTENSION_AUTOMATION_CONSTANTS_H_

namespace extension_automation_constants {

// All extension automation related messages will have this origin.
extern const char kAutomationOrigin[];
// Key used for all extension automation request types.
extern const wchar_t kAutomationRequestIdKey[];

// Keys used for API communications
extern const wchar_t kAutomationHasCallbackKey[];
extern const wchar_t kAutomationErrorKey[];  // not present implies success
extern const wchar_t kAutomationNameKey[];
extern const wchar_t kAutomationArgsKey[];
extern const wchar_t kAutomationResponseKey[];
// All external API requests have this target.
extern const char kAutomationRequestTarget[];
// All API responses should have this target.
extern const char kAutomationResponseTarget[];

// Keys used for port communications
extern const wchar_t kAutomationConnectionIdKey[];
extern const wchar_t kAutomationMessageDataKey[];
extern const wchar_t kAutomationExtensionIdKey[];
extern const wchar_t kAutomationPortIdKey[];
// All external port message requests should have this target.
extern const char kAutomationPortRequestTarget[];
// All external port message responses have this target.
extern const char kAutomationPortResponseTarget[];

// All external browser events have this target.
extern const char kAutomationBrowserEventRequestTarget[];

// The command codes for our private port protocol.
enum PrivatePortCommand {
  OPEN_CHANNEL = 0,
  CHANNEL_OPENED = 1,
  POST_MESSAGE = 2,
};

};  // namespace automation_extension_constants

#endif  // CHROME_BROWSER_AUTOMATION_EXTENSION_AUTOMATION_CONSTANTS_H_
