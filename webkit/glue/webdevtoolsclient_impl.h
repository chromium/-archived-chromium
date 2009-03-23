// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBDEVTOOLSCLIENT_IMPL_H_
#define WEBKIT_GLUE_WEBDEVTOOLSCLIENT_IMPL_H_

#include <string>

#include <wtf/OwnPtr.h>

#include "webkit/glue/cpp_bound_class.h"
#include "webkit/glue/devtools/devtools_rpc.h"
#include "webkit/glue/webdevtoolsclient.h"

namespace WebCore {
class String;
}

class JsDomAgentBoundObj;
class JsNetAgentBoundObj;
class JsToolsAgentBoundObj;
class WebDevToolsClientDelegate;
class WebViewImpl;

class WebDevToolsClientImpl : public WebDevToolsClient,
                              public CppBoundClass,
                              public DevToolsRpc::Delegate {
 public:
  WebDevToolsClientImpl(
      WebViewImpl* web_view_impl,
      WebDevToolsClientDelegate* delegate);
  virtual ~WebDevToolsClientImpl();

  // DevToolsRpc::Delegate implementation.
  virtual void SendRpcMessage(const std::string& raw_msg);

  // WebDevToolsClient implementation.
  virtual void DispatchMessageFromAgent(const std::string& raw_msg);

 private:
  void JsAddSourceToFrame(const CppArgumentList& args, CppVariant* result);

  WebViewImpl* web_view_impl_;
  WebDevToolsClientDelegate* delegate_;
  OwnPtr<JsDomAgentBoundObj> dom_agent_obj_;
  OwnPtr<JsNetAgentBoundObj> net_agent_obj_;
  OwnPtr<JsToolsAgentBoundObj> tools_agent_obj_;
  DISALLOW_COPY_AND_ASSIGN(WebDevToolsClientImpl);
};

#endif  // WEBKIT_GLUE_WEBDEVTOOLSCLIENT_IMPL_H_
