// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBDEVTOOLSCLIENT_H_
#define WEBKIT_GLUE_WEBDEVTOOLSCLIENT_H_

#include <string>
#include "base/basictypes.h"

class WebDevToolsClientDelegate;
class WebView;

// WebDevToolsClient represents DevTools client sitting in the Glue. It provides
// direct and delegate Apis to the host.
class WebDevToolsClient {
 public:
  static WebDevToolsClient* Create(
      WebView* view,
      WebDevToolsClientDelegate* delegate);

  WebDevToolsClient() {}
  virtual ~WebDevToolsClient() {}

  virtual void DispatchMessageFromAgent(const std::string& class_name,
                                        const std::string& method_name,
                                        const std::string& raw_msg) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebDevToolsClient);
};

#endif  // WEBKIT_GLUE_WEBDEVTOOLSCLIENT_H_
