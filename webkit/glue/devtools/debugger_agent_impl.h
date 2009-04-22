// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_DEVTOOLS_DEBUGGER_AGENT_IMPL_H_
#define WEBKIT_GLUE_DEVTOOLS_DEBUGGER_AGENT_IMPL_H_

#include <wtf/HashSet.h>

#include "v8.h"
#include "webkit/glue/devtools/debugger_agent.h"
#include "webkit/glue/webdevtoolsagent.h"

class WebDevToolsAgentImpl;
class WebViewImpl;

namespace WebCore {
class Document;
class Node;
class Page;
class String;
}

class DebuggerAgentImpl : public DebuggerAgent {
 public:
  DebuggerAgentImpl(WebViewImpl* web_view_impl,
                    DebuggerAgentDelegate* delegate,
                    WebDevToolsAgentImpl* webdevtools_agent);
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

  static void RunWithDeferredMessages(
      const HashSet<DebuggerAgentImpl*>& agents,
      WebDevToolsAgent::MessageLoopDispatchHandler handler);

  WebCore::Page* GetPage();
  WebDevToolsAgentImpl* webdevtools_agent() { return webdevtools_agent_; };

  WebViewImpl* web_view() { return web_view_impl_; }

 private:
  v8::Persistent<v8::Context> context_;
  WebViewImpl* web_view_impl_;
  DebuggerAgentDelegate* delegate_;
  WebDevToolsAgentImpl* webdevtools_agent_;

  DISALLOW_COPY_AND_ASSIGN(DebuggerAgentImpl);
};

#endif  // WEBKIT_GLUE_DEVTOOLS_DEBUGGER_AGENT_IMPL_H_
