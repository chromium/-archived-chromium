// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBDEVTOOLSCLIENT_DELEGATE_H_
#define WEBKIT_GLUE_WEBDEVTOOLSCLIENT_DELEGATE_H_

#include <string>
#include "base/basictypes.h"

class WebDevToolsClientDelegate {
 public:
  WebDevToolsClientDelegate() {}
  virtual ~WebDevToolsClientDelegate() {}

  virtual void SendMessageToAgent(const std::string& raw_msg) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebDevToolsClientDelegate);
};

#endif  // WEBKIT_GLUE_WEBDEVTOOLSAGENT_DELEGATE_H_
