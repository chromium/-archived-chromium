// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_DEVTOOLS_NET_AGENT_H_
#define WEBKIT_GLUE_DEVTOOLS_NET_AGENT_H_

#include "webkit/glue/devtools/devtools_rpc.h"

// NetAgent is a utility object that covers network-related functionality of the
// WebDevToolsAgent. It is capable of sniffing network calls and passing the
// HTTPRequest-related data to the client.
// NetAgent's environment is represented with the NetAgentDelegate interface.
#define NET_AGENT_STRUCT(METHOD0, METHOD1, METHOD2, METHOD3) \
  /* Requests that the agent sends content of the resource with given id to the
     delegate. */ \
  METHOD2(GetResourceContent, int /* identifier */, String /* url */)

DEFINE_RPC_CLASS(NetAgent, NET_AGENT_STRUCT)

#define NET_AGENT_DELEGATE_STRUCT(METHOD0, METHOD1, METHOD2, METHOD3) \
  /* Notifies the delegate that a request is about to be sent out. */ \
  METHOD2(WillSendRequest, int, Value) \
  \
  /* Notifies the delegate that response has been received. */ \
  METHOD2(DidReceiveResponse, int /* identifier */, Value /* request */) \
  \
  /* Notifies the delegate that resource loading has been finished with no
     errors */ \
  METHOD2(DidFinishLoading, int /* identifier */, Value /* response */) \
  \
  /* Notifies the delegate that resource loading has failed. */ \
  METHOD2(DidFailLoading, int /* identifier */, Value /* response */) \
  \
  /* Calls delegate back with requested resource content. */ \
  METHOD2(SetResourceContent, int /* identifier */, String /* content */)

DEFINE_RPC_CLASS(NetAgentDelegate, NET_AGENT_DELEGATE_STRUCT)

#endif  // WEBKIT_GLUE_DEVTOOLS_NET_AGENT_H_
