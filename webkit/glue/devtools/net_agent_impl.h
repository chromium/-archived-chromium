// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_DEVTOOLS_NET_AGENT_IMPL_H_
#define WEBKIT_GLUE_DEVTOOLS_NET_AGENT_IMPL_H_

#include <utility>

#include <wtf/HashMap.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

#include "webkit/glue/devtools/net_agent.h"

namespace WebCore {
class Document;
class DocumentLoader;
class HTTPHeaderMap;
class ResourceError;
class ResourceResponse;
class String;
struct ResourceRequest;
}

class Value;

// NetAgent is a utility object that covers network-related functionality of the
// WebDevToolsAgent. It is capable of sniffing network calls and passing the
// HttpRequest-related data to the client.
// NetAgent's environment is represented with the NetAgentDelegate interface.
class NetAgentImpl : public NetAgent {
 public:
  explicit NetAgentImpl(NetAgentDelegate* delegate);
  virtual ~NetAgentImpl();

  // Initializes net agent with the given document.
  void SetDocument(WebCore::Document* document);

  // Tells agent it has attached client.
  void Attach();

  // Tells agent it has no attached client.
  void Detach();

  // Tells agent that new load has been committed.
  void DidCommitMainResourceLoad();

  // NetAgent implementation.
  void GetResourceContent(int call_id, int identifier,
      const WebCore::String& request_url);
  void AssignIdentifierToRequest(
      WebCore::DocumentLoader* loader,
      int identifier,
      const WebCore::ResourceRequest& request);
  void WillSendRequest(
      WebCore::DocumentLoader* loader,
      int identifier,
      const WebCore::ResourceRequest& request);
  void DidReceiveResponse(
      WebCore::DocumentLoader* loader,
      int identifier,
      const WebCore::ResourceResponse &response);
  void DidReceiveContentLength(
      WebCore::DocumentLoader* loader,
      int identifier,
      int length);
  void DidFinishLoading(
      WebCore::DocumentLoader* loader,
      int identifier);
  void DidFailLoading(
      WebCore::DocumentLoader* loader,
      int identifier,
      const WebCore::ResourceError& error);
  void DidLoadResourceFromMemoryCache(
      WebCore::DocumentLoader* loader,
      const WebCore::ResourceRequest& request,
      const WebCore::ResourceResponse& response,
      int length);

 private:
  // Serializes headers map into a value.
  Value* BuildValueForHeaders(const WebCore::HTTPHeaderMap& headers);

  NetAgentDelegate* delegate_;
  WebCore::Document* document_;
  RefPtr<WebCore::DocumentLoader> main_loader_;
  typedef HashMap<int, DictionaryValue*, DefaultHash<int>::Hash,
                  WTF::UnsignedWithZeroKeyHashTraits<int> > ResourcesMap;
  typedef Vector<std::pair<int, DictionaryValue*> > FinishedResources;
  ResourcesMap pending_resources_;
  FinishedResources finished_resources_;
  int last_cached_identifier_;
  bool attached_;
  DISALLOW_COPY_AND_ASSIGN(NetAgentImpl);
};

#endif  // WEBKIT_GLUE_DEVTOOLS_NET_AGENT_IMPL_H_
