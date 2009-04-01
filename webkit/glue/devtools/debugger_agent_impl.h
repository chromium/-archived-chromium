// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_DEVTOOLS_DEBUGGER_AGENT_IMPL_H_
#define WEBKIT_GLUE_DEVTOOLS_DEBUGGER_AGENT_IMPL_H_

#include "v8.h"
#include "webkit/glue/devtools/debugger_agent.h"

namespace WebCore {
class Document;
class Node;
class String;
}

class DebuggerAgentImpl : public DebuggerAgent {
 public:
  explicit DebuggerAgentImpl(DebuggerAgentDelegate* delegate);
  virtual ~DebuggerAgentImpl();

  // Initializes dom agent with the given document.
  void SetDocument(WebCore::Document* document);

  // DebuggerAgent implementation.
  virtual void DebugBreak();

  void DebuggerOutput(const std::string& out);

  // Executes utility function with the given node and json
  // args as parameters. These functions must be implemented in
  // the inject.js file.
  WebCore::String ExecuteUtilityFunction(
      const WebCore::String& function_name,
      WebCore::Node* node,
      const WebCore::String& json_args);

 private:
  v8::Persistent<v8::Context> context_;
  DebuggerAgentDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(DebuggerAgentImpl);
};

#endif  // WEBKIT_GLUE_DEVTOOLS_DEBUGGER_AGENT_IMPL_H_
