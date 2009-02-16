// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Developer tools consist of following parts: 
//
// ToolsAgent lives in the renderer of an inspected page and provides access to
// the pages resources, DOM, v8 etc. by means of IPC messages. 
// 
// ToolsClient is a thin delegate that lives in the tools front-end renderer and
// converts IPC messages to frontend methods calls and allows the frontend to 
// send messages to the ToolsAgent.
//
// All the messages are routed through browser process.
//
// Chain of communication between the components may be described by the 
// following diagram:
//  --------------------------
// | (tools frontend          |
// | renderer process)        |
// |                          |            --------------------
// |tools     <--> ToolsClient+<-- IPC -->+ (browser process)  |
// |frontend                  |           |                    |
//  --------------------------             -----------+--------
//                                                    ^
//                                                    |
//                                                   IPC
//                                                    |
//                                                    v
//                          --------------------------+-------
//                         | inspected page <--> ToolsAgent   |
//                         |                                  |
//                         | (inspected page renderer process)|
//                          ----------------------------------
// 
// This file describes interface between tools frontend and ToolsClient in the 
// above diagram.

#ifndef WEBKIT_GLUE_TOOLS_PROXY_H_
#define WEBKIT_GLUE_TOOLS_PROXY_H_

#include "base/basictypes.h"

class ToolsUI;

// Interface for sending messages to remote ToolsAgent.
class ToolsProxy {
 public:
  ToolsProxy() {}
  virtual ~ToolsProxy() {}

  virtual void SetToolsUI(ToolsUI*) = 0;

  virtual void DebugAttach() = 0;
  virtual void DebugDetach() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(ToolsProxy);
};


// Interface for accessing tools frontend.
class ToolsUI {
 public:
  ToolsUI() {}
  virtual ~ToolsUI() {}

  virtual void OnDidDebugAttach() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(ToolsUI);
};

#endif // WEBKIT_GLUE_TOOLS_PROXY_H_
