// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_AUTOMATION_AUTOMATION_CONSTANTS_H__
#define CHROME_TEST_AUTOMATION_AUTOMATION_CONSTANTS_H__

namespace automation {
  // Amount of time to wait before querying the browser.
  static const int kSleepTime = 250;
}

enum AutomationMsg_NavigationResponseValues {
  AUTOMATION_MSG_NAVIGATION_ERROR = 0,
  AUTOMATION_MSG_NAVIGATION_SUCCESS,
  AUTOMATION_MSG_NAVIGATION_AUTH_NEEDED,
};

#endif  // CHROME_TEST_AUTOMATION_AUTOMATION_CONSTANTS_H__
