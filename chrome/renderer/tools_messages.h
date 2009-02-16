// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_TOOLS_MESSAGES_H_
#define CHROME_RENDERER_TOOLS_MESSAGES_H_

enum ToolsAgentMessageType {
  TOOLS_AGENT_MSG_DEBUG_ATTACH = 0,
  TOOLS_AGENT_MSG_DEBUG_BREAK,
  TOOLS_AGENT_MSG_DEBUG_COMMAND,
  TOOLS_AGENT_MSG_DEBUG_DETACH
};

enum ToolsClientMessageType {
  TOOLS_CLIENT_MSG_ADD_MESSAGE_TO_CONSOLE = 0,
  TOOLS_CLIENT_MSG_DEBUGGER_OUTPUT,
  TOOLS_CLIENT_MSG_DID_DEBUG_ATTACH
};

#endif // CHROME_RENDERER_TOOLS_MESSAGES_H_
