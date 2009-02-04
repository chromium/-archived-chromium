// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_AUTOMATION_AUTOMATION_MESSAGES_H__
#define CHROME_TEST_AUTOMATION_AUTOMATION_MESSAGES_H__

#include <string>

#include "base/basictypes.h"
#include "chrome/common/ipc_message_utils.h"

enum AutomationMsg_NavigationResponseValues {
  AUTOMATION_MSG_NAVIGATION_ERROR = 0,
  AUTOMATION_MSG_NAVIGATION_SUCCESS,
  AUTOMATION_MSG_NAVIGATION_AUTH_NEEDED,
};

#define MESSAGES_INTERNAL_FILE \
    "chrome/test/automation/automation_messages_internal.h"
#include "chrome/common/ipc_message_macros.h"

#endif  // CHROME_TEST_AUTOMATION_AUTOMATION_MESSAGES_H__

