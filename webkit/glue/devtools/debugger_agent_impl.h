// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_DEVTOOLS_DEBUGGER_AGENT_IMPL_H_
#define WEBKIT_GLUE_DEVTOOLS_DEBUGGER_AGENT_IMPL_H_

#include "webkit/glue/devtools/debugger_agent.h"

class DebuggerAgentImpl : public DebuggerAgent {
 public:
  explicit DebuggerAgentImpl(DebuggerAgentDelegate* delegate);
  virtual ~DebuggerAgentImpl();

  // DebuggerAgent implementation.
  virtual void DebugBreak();

  void DebuggerOutput(const std::string& out);

 private:

  DebuggerAgentDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(DebuggerAgentImpl);
};

#endif  // WEBKIT_GLUE_DEVTOOLS_DEBUGGER_AGENT_IMPL_H_
