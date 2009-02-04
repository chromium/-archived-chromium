// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/ipc_message.h"

#ifdef IPC_MESSAGE_LOG_ENABLED

// Preprocessor magic: render_messages.h defines the enums and debug string
// functions if this define is set.
#define IPC_MESSAGE_MACROS_LOG_ENABLED

#endif // IPC_MESSAGE_LOG_ENABLED

#include "chrome/common/render_messages.h"

void RenderMessagesInit() {
#ifdef IPC_MESSAGE_LOG_ENABLED
  IPC::RegisterMessageLogger(ViewStart, ViewMsgLog);
  IPC::RegisterMessageLogger(ViewHostStart, ViewHostMsgLog);
#endif
}
