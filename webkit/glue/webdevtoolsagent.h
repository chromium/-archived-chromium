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
  WebDevToolsAgent() {}
  virtual ~WebDevToolsAgent() {}

  virtual void DispatchMessageFromClient(const std::string& raw_msg) = 0;
 private:
  DISALLOW_COPY_AND_ASSIGN(WebDevToolsAgent);
};

#endif  // WEBKIT_GLUE_WEBDEVTOOLSAGENT_H_
