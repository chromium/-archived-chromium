// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBDEVTOOLSAGENT_H_
#define WEBKIT_GLUE_WEBDEVTOOLSAGENT_H_

#include <string>
#include "base/basictypes.h"

// WebDevToolsAgent represents DevTools agent sitting in the Glue. It provides
// direct and delegate Apis to the host.
class WebDevToolsAgent {
 public:
  class Message {
   public:
    Message() {}
    virtual ~Message() {}
    virtual void Dispatch() = 0;
   private:
    DISALLOW_COPY_AND_ASSIGN(Message);
  };

  WebDevToolsAgent() {}
  virtual ~WebDevToolsAgent() {}

  virtual void Attach() = 0;

  virtual void Detach() = 0;

  virtual void DispatchMessageFromClient(const std::string& class_name,
                                         const std::string& method_name,
                                         const std::string& raw_msg) = 0;

  virtual void InspectElement(int x, int y) = 0;

  // Asynchronously executes debugger command in the render thread.
  // |caller_id| will be used for sending response.
  static void ExecuteDebuggerCommand(const std::string& command,
                                     int caller_id);

  typedef void (*MessageLoopDispatchHandler)();

  // Installs dispatch handle that is going to be called periodically
  // while on a breakpoint.
  static void SetMessageLoopDispatchHandler(
      MessageLoopDispatchHandler handler);

 private:
  DISALLOW_COPY_AND_ASSIGN(WebDevToolsAgent);
};

#endif  // WEBKIT_GLUE_WEBDEVTOOLSAGENT_H_
