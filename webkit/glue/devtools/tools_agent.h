// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_DEVTOOLS_TOOLS_AGENT_H_
#define WEBKIT_GLUE_DEVTOOLS_TOOLS_AGENT_H_

#include "webkit/glue/devtools/devtools_rpc.h"

// Tools agent provides API for enabling / disabling other agents as well as
// API for auxiliary UI functions such as dom elements highlighting.
#define TOOLS_AGENT_STRUCT(METHOD0, METHOD1, METHOD2, METHOD3) \
  /* Enables / disables Dom agent. */ \
  METHOD1(SetDomAgentEnabled, bool /* enabled */) \
  \
  /* Enables / disables Net agent */ \
  METHOD1(SetNetAgentEnabled, bool /* enabled */) \
  \
  /* Highlights Dom node with given ID */ \
  METHOD1(HighlightDOMNode, int /* node_id */) \
  \
  /* Clears Dom Node highlight. */ \
  METHOD0(HideDOMNodeHighlight)

DEFINE_RPC_CLASS(ToolsAgent, TOOLS_AGENT_STRUCT)

#define TOOLS_AGENT_DELEGATE_STRUCT(METHOD0, METHOD1, METHOD2, METHOD3) \
  METHOD1(UpdateFocusedNode, int /* node_id */) \

DEFINE_RPC_CLASS(ToolsAgentDelegate, TOOLS_AGENT_DELEGATE_STRUCT)

#endif  // WEBKIT_GLUE_DEVTOOLS_TOOLS_AGENT_H_
