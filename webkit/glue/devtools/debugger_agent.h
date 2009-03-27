// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_DEVTOOLS_DEBUGGER_AGENT_H_
#define WEBKIT_GLUE_DEVTOOLS_DEBUGGER_AGENT_H_

#include "webkit/glue/devtools/devtools_rpc.h"

#define DEBUGGER_AGENT_STRUCT(METHOD0, METHOD1, METHOD2, METHOD3) \
  /* Stops v8 execution as soon as it gets control. */ \
  METHOD0(DebugBreak)

DEFINE_RPC_CLASS(DebuggerAgent, DEBUGGER_AGENT_STRUCT)

#define DEBUGGER_AGENT_DELEGATE_STRUCT(METHOD0, METHOD1, METHOD2, METHOD3) \
  METHOD1(DebuggerOutput, std::string /* output text */)

DEFINE_RPC_CLASS(DebuggerAgentDelegate, DEBUGGER_AGENT_DELEGATE_STRUCT)

#endif  // WEBKIT_GLUE_DEVTOOLS_DEBUGGER_AGENT_H_
