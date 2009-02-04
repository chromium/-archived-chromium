// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_AUTOMATION_AUTOMATION_MESSAGES_H__
#define CHROME_TEST_AUTOMATION_AUTOMATION_MESSAGES_H__

#include <string>

#include "base/basictypes.h"
#include "chrome/common/ipc_message.h"
#include "chrome/common/ipc_message_utils.h"

enum AutomationMsg_NavigationResponseValues {
  AUTOMATION_MSG_NAVIGATION_ERROR = 0,
  AUTOMATION_MSG_NAVIGATION_SUCCESS,
  AUTOMATION_MSG_NAVIGATION_AUTH_NEEDED,
};

// Two-pass include of render_messages_internal.  Preprocessor magic allows
// us to use 1 header to define the enums and classes for our render messages.
#define IPC_MESSAGE_MACROS_ENUMS
#include "chrome/test/automation/automation_messages_internal.h"

#ifdef IPC_MESSAGE_MACROS_LOG_ENABLED
#  undef IPC_MESSAGE_MACROS_LOG
#  define IPC_MESSAGE_MACROS_CLASSES
#include "chrome/test/automation/automation_messages_internal.h"

#  undef IPC_MESSAGE_MACROS_CLASSES
#  define IPC_MESSAGE_MACROS_LOG
#include "chrome/test/automation/automation_messages_internal.h"
#else
// No debug strings requested, just define the classes
#  define IPC_MESSAGE_MACROS_CLASSES
#include "chrome/test/automation/automation_messages_internal.h"
#endif


#endif  // CHROME_TEST_AUTOMATION_AUTOMATION_MESSAGES_H__

