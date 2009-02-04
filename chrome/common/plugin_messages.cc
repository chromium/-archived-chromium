// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/ipc_message.h"

#ifdef IPC_MESSAGE_LOG_ENABLED

#define IPC_MESSAGE_MACROS_LOG_ENABLED

#endif // IPC_MESSAGE_LOG_ENABLED

#include "chrome/common/plugin_messages.h"

void PluginMessagesInit() {
#ifdef IPC_MESSAGE_LOG_ENABLED
  IPC::RegisterMessageLogger(PluginProcessStart, PluginProcessMsgLog);
  IPC::RegisterMessageLogger(PluginProcessHostStart, PluginProcessHostMsgLog);
  IPC::RegisterMessageLogger(PluginStart, PluginMsgLog);
  IPC::RegisterMessageLogger(PluginHostStart, PluginHostMsgLog);
  IPC::RegisterMessageLogger(NPObjectStart, NPObjectMsgLog);
#endif
}
