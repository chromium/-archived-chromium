// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_DEVTOOLS_DEBUGGER_AGENT_H_
#define WEBKIT_GLUE_DEVTOOLS_DEBUGGER_AGENT_H_

#include <string>

#include "webkit/glue/devtools/devtools_rpc.h"

#define DEBUGGER_AGENT_STRUCT(METHOD0, METHOD1, METHOD2, METHOD3, \
    METHOD4) \
  /* Stops v8 execution as soon as it gets control. */ \
  METHOD0(DebugBreak) \
  \
  /* Requests global context id of the inspected tab. */ \
  METHOD0(GetContextId) \
  \
  /* Starts profiling (samples collection). */ \
  METHOD0(StartProfiling) \
  \
  /* Stops profiling (samples collection). */ \
  METHOD0(StopProfiling) \
  \
  /* Requests current profiler status. */ \
  METHOD0(IsProfilingStarted) \
  \
  /* Retrieves next portion of profiler log. */ \
  METHOD0(GetNextLogLines)

DEFINE_RPC_CLASS(DebuggerAgent, DEBUGGER_AGENT_STRUCT)

#define DEBUGGER_AGENT_DELEGATE_STRUCT(METHOD0, METHOD1, METHOD2, METHOD3, \
    METHOD4) \
  METHOD1(DebuggerOutput, std::string /* output text */) \
  \
  /* Pushes debugger context id into the client. */ \
  METHOD1(SetContextId, int /* context id */) \
  \
  /* Response to IsProfilingStarted. */ \
  METHOD1(DidIsProfilingStarted, bool /* is_started */) \
  \
  /* Response to GetNextLogLines. */ \
  METHOD1(DidGetNextLogLines, std::string /* log */)

DEFINE_RPC_CLASS(DebuggerAgentDelegate, DEBUGGER_AGENT_DELEGATE_STRUCT)

#endif  // WEBKIT_GLUE_DEVTOOLS_DEBUGGER_AGENT_H_
