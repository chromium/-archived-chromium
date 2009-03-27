// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include <wtf/Assertions.h>
#undef LOG

#include "webkit/glue/devtools/debugger_agent_impl.h"
#include "webkit/glue/devtools/debugger_agent_manager.h"

DebuggerAgentImpl::DebuggerAgentImpl(DebuggerAgentDelegate* delegate)
    : delegate_(delegate) {
  DebuggerAgentManager::DebugAttach(this);
}

DebuggerAgentImpl::~DebuggerAgentImpl() {
  DebuggerAgentManager::DebugDetach(this);
}

void DebuggerAgentImpl::DebugBreak() {
  // TODO(yurys): implement
}

void DebuggerAgentImpl::DebuggerOutput(const std::string& command) {
  delegate_->DebuggerOutput(command);
}
