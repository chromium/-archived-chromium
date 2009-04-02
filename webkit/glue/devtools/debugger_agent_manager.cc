// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include <wtf/HashSet.h>
#undef LOG

#include "base/string_util.h"
#include "webkit/glue/devtools/debugger_agent_impl.h"
#include "webkit/glue/devtools/debugger_agent_manager.h"

#if USE(V8)
#include "v8/include/v8-debug.h"
#endif

// static
void DebuggerAgentManager::V8DebugMessageHandler(const uint16_t* message,
                                                 int length,
                                                 void* data) {
#if USE(V8)
  std::wstring out(reinterpret_cast<const wchar_t*>(message), length);
  std::string out_utf8 = WideToUTF8(out);
  DebuggerAgentManager::DebuggerOutput(out_utf8);
#endif
}

// static
DebuggerAgentManager::AttachedAgentsSet*
    DebuggerAgentManager::attached_agents_ = NULL;

// static
void DebuggerAgentManager::DebugAttach(DebuggerAgentImpl* debugger_agent) {
#if USE(V8)
  if (!attached_agents_) {
    attached_agents_ = new AttachedAgentsSet();
    v8::Debug::SetMessageHandler(
        &DebuggerAgentManager::V8DebugMessageHandler,
        NULL, /* no additional data */
        false /* don't create separate thread for sending debugger output */);
  }
  attached_agents_->add(debugger_agent);
#endif
}

// static
void DebuggerAgentManager::DebugDetach(DebuggerAgentImpl* debugger_agent) {
#if USE(V8)
  if (!attached_agents_) {
    NOTREACHED();
    return;
  }
  DCHECK(attached_agents_->contains(debugger_agent));
  attached_agents_->remove(debugger_agent);
  if (attached_agents_->isEmpty()) {
    v8::Debug::SetMessageHandler(NULL);
    delete attached_agents_;
    attached_agents_ = NULL;
  }
#endif
}

// static
void DebuggerAgentManager::DebugBreak(DebuggerAgentImpl* debugger_agent) {
#if USE(V8)
  DCHECK(attached_agents_->contains(debugger_agent));
  v8::Debug::DebugBreak();
#endif
}

// static
void DebuggerAgentManager::DebuggerOutput(const std::string& out) {
  DebuggerAgentImpl* agent = GetAgentForCurrentV8Context();
  if (!agent) {
    return;
  }
  agent->DebuggerOutput(out);
}

// static
void DebuggerAgentManager::ExecuteDebuggerCommand(const std::string& command) {
#if USE(V8)
  std::wstring cmd_wstring = UTF8ToWide(command);
  v8::Debug::SendCommand(reinterpret_cast<const uint16_t*>(cmd_wstring.data()),
                         cmd_wstring.length());
#endif
}

// static
DebuggerAgentImpl* DebuggerAgentManager::GetAgentForCurrentV8Context() {
  if (!attached_agents_) {
    return NULL;
  }
  DCHECK(!attached_agents_->isEmpty());
  // TODO(yurys): find agent for current v8 global context. Now we return first
  // agent in the set.
  return *attached_agents_->begin();
}
