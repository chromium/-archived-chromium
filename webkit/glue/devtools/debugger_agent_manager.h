// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_DEVTOOLS_DEBUGGER_AGENT_MANAGER_H_
#define WEBKIT_GLUE_DEVTOOLS_DEBUGGER_AGENT_MANAGER_H_

#include <wtf/HashSet.h>

#include "base/basictypes.h"
#include "v8/include/v8-debug.h"

class DebuggerAgentImpl;

// There is single v8 instance per render process. Also there may be several
// RenderViews and consequently devtools agents in the process that want to talk
// to the v8 debugger. This class coordinates communication between the debug
// agents and v8 debugger. It will set debug output handler as long as at least
// one debugger agent is attached and remove it when last debugger agent is
// detached. When message is received from debugger it will route it to the
// right debugger agent if there is one otherwise the message will be ignored.
//
// TODO(yurys): v8 may send a message(e.g. exception event) after which it
// would expect some actions from the handler. If there is no appropriate
// debugger agent to handle such messages the manager should perform the action
// itself, otherwise v8 may hang waiting for the action.
class DebuggerAgentManager {
 public:
  static void DebugAttach(DebuggerAgentImpl* debugger_agent);
  static void DebugDetach(DebuggerAgentImpl* debugger_agent);
  static void DebugCommand(const std::string& command);

  static void ExecuteDebuggerCommand(const std::string& command);

 private:
  DebuggerAgentManager();
  ~DebuggerAgentManager();

  static void V8DebugMessageHandler(const uint16_t* message,
                                    int length,
                                    void* data);
  static void DebuggerOutput(const std::string& out);
  static DebuggerAgentImpl* GetAgentForCurrentV8Context();

  typedef HashSet<DebuggerAgentImpl*> AttachedAgentsSet;
  static AttachedAgentsSet* attached_agents_;

  DISALLOW_COPY_AND_ASSIGN(DebuggerAgentManager);
};

#endif  // WEBKIT_GLUE_DEVTOOLS_DEBUGGER_AGENT_MANAGER_H_
