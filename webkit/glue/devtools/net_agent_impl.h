// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_DEVTOOLS_NET_AGENT_IMPL_H_
#define WEBKIT_GLUE_DEVTOOLS_NET_AGENT_IMPL_H_

#include <utility>

#include "HTTPHeaderMap.h"
#include "KURL.h"
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
class ScriptString;
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
  void DidLoadResourceByXMLHttpRequest(
    int identifier,
    const WebCore::ScriptString& source);

 private:
  struct Resource {
    Resource()
        : main_resource(false),
          start_time(0),
          response_received_time(0),
          end_time(0),
          expected_content_length(0),
          http_status_code(0),
          error_code(0) {
    }
    bool main_resource;

    double start_time;
    double response_received_time;
    double end_time;

    WebCore::KURL url;
    WebCore::String mime_type;
    WebCore::String suggested_filename;

    int expected_content_length;
    int http_status_code;

    WebCore::HTTPHeaderMap request_headers;
    WebCore::HTTPHeaderMap response_headers;

    int error_code;
    WebCore::String error_description;
  };

  static void Serialize(const Resource& resource, DictionaryValue* value);

  // Serializes headers map into a value.
  static Value* BuildValueForHeaders(const WebCore::HTTPHeaderMap& headers);

  void ExpireFinishedResourcesCache();

  NetAgentDelegate* delegate_;
  WebCore::Document* document_;
  RefPtr<WebCore::DocumentLoader> main_loader_;
  typedef HashMap<int, Resource*, DefaultHash<int>::Hash,
                  WTF::UnsignedWithZeroKeyHashTraits<int> > ResourcesMap;
  typedef Vector<std::pair<int, Resource*> > FinishedResources;
  typedef HashMap<int, WebCore::ScriptString, DefaultHash<int>::Hash,
                  WTF::UnsignedWithZeroKeyHashTraits<int> > XmlHttpSources;

  ResourcesMap pending_resources_;
  FinishedResources finished_resources_;
  XmlHttpSources xml_http_sources_;
  int last_cached_identifier_;
  bool attached_;
  DISALLOW_COPY_AND_ASSIGN(NetAgentImpl);
};

#endif  // WEBKIT_GLUE_DEVTOOLS_NET_AGENT_IMPL_H_
