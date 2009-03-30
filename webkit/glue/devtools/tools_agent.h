// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_DEVTOOLS_TOOLS_AGENT_H_
#define WEBKIT_GLUE_DEVTOOLS_TOOLS_AGENT_H_

#include "webkit/glue/devtools/devtools_rpc.h"

// Tools agent provides API for enabling / disabling other agents as well as
// API for auxiliary UI functions such as dom elements highlighting.
#define TOOLS_AGENT_STRUCT(METHOD0, METHOD1, METHOD2, METHOD3, METHOD4) \
  /* Highlights Dom node with given ID */ \
  METHOD1(HighlightDOMNode, int /* node_id */) \
  \
  /* Clears Dom Node highlight. */ \
  METHOD0(HideDOMNodeHighlight) \
  \
  /* Executes JavaScript in the context of the inspected window. */ \
  METHOD2(EvaluateJavaSctipt, int /* call_id */, String /* JS expression */)

DEFINE_RPC_CLASS(ToolsAgent, TOOLS_AGENT_STRUCT)

#define TOOLS_AGENT_DELEGATE_STRUCT(METHOD0, METHOD1, METHOD2, METHOD3, \
    METHOD4) \
  /* Updates focused node on the client. */ \
  METHOD1(UpdateFocusedNode, int /* node_id */) \
  \
  /* Response message to EvaluateJavaSctipt. */ \
  METHOD2(DidEvaluateJavaSctipt, int /* call_id */, String /* result */) \
  \
  /* Updates focused node on the client. */ \
  METHOD2(FrameNavigate, std::string /* url */, bool /* top_level */)

DEFINE_RPC_CLASS(ToolsAgentDelegate, TOOLS_AGENT_DELEGATE_STRUCT)

#endif  // WEBKIT_GLUE_DEVTOOLS_TOOLS_AGENT_H_
