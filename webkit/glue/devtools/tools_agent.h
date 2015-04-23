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
  /* Requests that utility js function is executed with the given args. */ \
  METHOD3(ExecuteUtilityFunction, int /* call_id */, \
      String /* function_name */, String /* json_args */) \
  \
  /* Clears cached console messages. */ \
  METHOD0(ClearConsoleMessages) \
  \
  /* Requests that the agent sends content of the resource with given id to the
     delegate. */ \
  METHOD2(GetResourceContent, int /* call_id */, int /* identifier */) \
  \
  /* Turns resource tracking on / off. */ \
  METHOD2(SetResourceTrackingEnabled, bool /* enabled */, bool /* always */)

DEFINE_RPC_CLASS(ToolsAgent, TOOLS_AGENT_STRUCT)

#define TOOLS_AGENT_DELEGATE_STRUCT(METHOD0, METHOD1, METHOD2, METHOD3, \
    METHOD4) \
  /* Updates focused node on the client. */ \
  METHOD1(UpdateFocusedNode, int /* node_id */) \
  \
  /* Updates focused node on the client. */ \
  METHOD1(FrameNavigate, std::string /* url */) \
  \
  /* Response to the GetNodeProperties. */ \
  METHOD3(DidExecuteUtilityFunction, int /* call_id */, String /* result */, \
      String /* exception */) \
  \
  /* Sends InspectorFrontend message to be dispatched on client. */ \
  METHOD1(DispatchOnClient, String /* data */) \
  \
  /* Tells frontend if resources panel should be enabled in the UI. */ \
  METHOD1(SetResourcesPanelEnabled, bool /* enabled. */)

DEFINE_RPC_CLASS(ToolsAgentDelegate, TOOLS_AGENT_DELEGATE_STRUCT)

#define TOOLS_AGENT_NATIVE_DELEGATE_STRUCT(METHOD0, METHOD1, METHOD2, METHOD3, \
    METHOD4) \
  /* Response to the async call. */ \
  METHOD2(DidGetResourceContent, int /* call_id */, String /* content */) \

DEFINE_RPC_CLASS(ToolsAgentNativeDelegate, TOOLS_AGENT_NATIVE_DELEGATE_STRUCT)

#endif  // WEBKIT_GLUE_DEVTOOLS_TOOLS_AGENT_H_
