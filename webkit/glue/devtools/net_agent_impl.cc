// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "CachedCSSStyleSheet.h"
#include "CachedResource.h"
#include "CachedScript.h"
#include "CachedXSLStyleSheet.h"
#include "DocLoader.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "FrameLoader.h"
#include "PlatformString.h"
#include "ResourceError.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "ScriptString.h"
#include "TextEncoding.h"
#include <wtf/CurrentTime.h>
#undef LOG

#include "base/basictypes.h"
#include "base/values.h"
#include "googleurl/src/gurl.h"
#include "webkit/glue/devtools/net_agent_impl.h"
#include "webkit/glue/glue_util.h"

using namespace WebCore;

NetAgentImpl::NetAgentImpl(NetAgentDelegate* delegate)
    : delegate_(delegate),
      document_(NULL),
      main_loader_(NULL),
      last_cached_identifier_(-2),
      attached_(false)  {
}

NetAgentImpl::~NetAgentImpl() {
  SetDocument(NULL);
  DidCommitMainResourceLoad();
  deleteAllValues(pending_resources_);
  pending_resources_.clear();
}

void NetAgentImpl::SetDocument(Document* doc) {
  document_ = doc;
}

void NetAgentImpl::Attach() {
  for (FinishedResources::iterator it = finished_resources_.begin();
       it != finished_resources_.end(); ++it) {
    DictionaryValue value;
    Serialize(*it->second, &value);
    delegate_->DidFinishLoading(it->first, value);
  }
  attached_ = true;
}

void NetAgentImpl::Detach() {
  attached_ = false;
  xml_http_sources_.clear();
  ExpireFinishedResourcesCache();
}

void NetAgentImpl::DidCommitMainResourceLoad() {
  for (FinishedResources::iterator it = finished_resources_.begin();
       it != finished_resources_.end(); ++it) {
    delete it->second;
  }
  finished_resources_.clear();
  main_loader_ = NULL;
}

void NetAgentImpl::AssignIdentifierToRequest(
    DocumentLoader* loader,
    int identifier,
    const ResourceRequest& request) {
}

void NetAgentImpl::WillSendRequest(
    DocumentLoader* loader,
    int identifier,
    const ResourceRequest& request) {
  if (pending_resources_.contains(identifier)) {
    // We are going through redirect, nuke old resource.
    delete pending_resources_.get(identifier);
  }

  Resource* resource = new Resource();
  pending_resources_.set(identifier, resource);
  KURL url = request.url();
  resource->start_time = WTF::currentTime();
  resource->url = request.url();
  resource->request_headers = request.httpHeaderFields();

  if (attached_) {
    DictionaryValue value;
    Serialize(*resource, &value);
    delegate_->WillSendRequest(identifier, value);
  }
}

void NetAgentImpl::DidReceiveResponse(
    DocumentLoader* loader,
    int identifier,
    const ResourceResponse &response) {
  if (!pending_resources_.contains(identifier)) {
    return;
  }

  KURL url = response.url();
  Resource* resource = pending_resources_.get(identifier);
  resource->response_received_time = WTF::currentTime();
  resource->expected_content_length =
      static_cast<int>(response.expectedContentLength());
  resource->http_status_code = response.httpStatusCode();
  resource->mime_type = response.mimeType();
  resource->suggested_filename = response.suggestedFilename();
  resource->response_headers = response.httpHeaderFields();

  if (attached_) {
    DictionaryValue value;
    Serialize(*resource, &value);
    delegate_->DidReceiveResponse(identifier, value);
  }
}

void NetAgentImpl::DidReceiveContentLength(
    DocumentLoader* loader,
    int identifier,
    int length) {
}

void NetAgentImpl::DidFinishLoading(
    DocumentLoader* loader,
    int identifier) {
  if (!pending_resources_.contains(identifier)) {
    return;
  }

  Resource* resource = pending_resources_.get(identifier);
  resource->end_time = WTF::currentTime();

  // This is the first command being dispatched after
  // DidCommitMainResourceLoad, we know that the first resource to be reported
  // as loaded is main resource.
  if (!main_loader_.get()) {
    main_loader_ = loader;
    resource->main_resource = true;
  }

  pending_resources_.remove(identifier);
  finished_resources_.append(std::make_pair(identifier, resource));

  if (attached_) {
    DictionaryValue value;
    Serialize(*resource, &value);
    delegate_->DidFinishLoading(identifier, value);
  } else {
    ExpireFinishedResourcesCache();
  }
}

void NetAgentImpl::DidFailLoading(
    DocumentLoader* loader,
    int identifier,
    const ResourceError& error) {
  if (!pending_resources_.contains(identifier)) {
    return;
  }
  Resource* resource = pending_resources_.get(identifier);
  resource->error_code = error.errorCode();
  resource->error_description = error.localizedDescription();
  DidFinishLoading(loader, identifier);
}

void NetAgentImpl::DidLoadResourceFromMemoryCache(
    DocumentLoader* loader,
    const ResourceRequest& request,
    const ResourceResponse& response,
    int length) {
  int identifier = last_cached_identifier_--;
}

void NetAgentImpl::DidLoadResourceByXMLHttpRequest(
    int identifier,
    const WebCore::ScriptString& source) {
  if (attached_) {
    // Only store XmlHttpRequests data when client is attached.
    xml_http_sources_.set(identifier, source);
  }
}

void NetAgentImpl::GetResourceContent(
    int call_id,
    int identifier,
    const String& url) {
  if (!document_) {
    return;
  }

  String source;

  WebCore::ScriptString script = xml_http_sources_.get(identifier);
  if (!script.isNull()) {
    source = String(script);
  } else if (main_loader_.get() && main_loader_->requestURL() == url) {
    RefPtr<SharedBuffer> buffer = main_loader_->mainResourceData();
    String text_encoding_name = document_->inputEncoding();
    if (buffer) {
      WebCore::TextEncoding encoding(text_encoding_name);
      if (!encoding.isValid())
        encoding = WindowsLatin1Encoding();
      source = encoding.decode(buffer->data(), buffer->size());
    }
  } else {
    CachedResource* cached_resource = document_->
        docLoader()->cachedResource(url);
    if (!cached_resource) {
      delegate_->GetResourceContentResult(call_id, "");
      return;
    }
    if (cached_resource->isPurgeable()) {
      // If the resource is purgeable then make it unpurgeable to get its data.
      // This might fail, in which case we return an empty string.
      if (!cached_resource->makePurgeable(false)) {
        delegate_->GetResourceContentResult(call_id, "");
        return;
      }
    }

    // Try to get the decoded source.  Only applies to some CachedResource
    // types.
    switch (cached_resource->type()) {
      case CachedResource::CSSStyleSheet: {
        CachedCSSStyleSheet *sheet =
            reinterpret_cast<CachedCSSStyleSheet*>(cached_resource);
        source = sheet->sheetText();
        break;
      }
      case CachedResource::Script: {
        CachedScript *script =
            reinterpret_cast<CachedScript*>(cached_resource);
        source = script->script();
        break;
      }
#if ENABLE(XSLT)
      case CachedResource::XSLStyleSheet: {
        CachedXSLStyleSheet *sheet =
            reinterpret_cast<CachedXSLStyleSheet*>(cached_resource);
        source = sheet->sheet();
        break;
      }
#endif
      default:
        break;
    }
  }
  delegate_->GetResourceContentResult(call_id,
      webkit_glue::StringToStdString(source));
}

// static
Value* NetAgentImpl::BuildValueForHeaders(const HTTPHeaderMap& headers) {
  OwnPtr<DictionaryValue> value(new DictionaryValue());
  HTTPHeaderMap::const_iterator end = headers.end();
  for (HTTPHeaderMap::const_iterator it = headers.begin(); it != end; ++it) {
    value->SetString(webkit_glue::StringToStdWString(it->first),
        webkit_glue::StringToStdString(it->second));
  }
  return value.release();
}

// static
void NetAgentImpl::Serialize(const Resource& resource,
                             DictionaryValue* value) {
  value->SetReal(L"startTime", resource.start_time);
  value->SetReal(L"responseReceivedTime", resource.response_received_time);
  value->SetReal(L"endTime", resource.end_time);

  value->SetString(L"requestURL",
                   webkit_glue::StringToStdString(resource.url.string()));
  value->SetString(L"host",
                   webkit_glue::StringToStdString(resource.url.host()));
  value->SetString(L"path",
                   webkit_glue::StringToStdString(resource.url.path()));
  value->SetString(
      L"lastPathComponent",
      webkit_glue::StringToStdString(resource.url.lastPathComponent()));

  value->SetString(L"mimeType",
      webkit_glue::StringToStdWString(resource.mime_type));
  value->SetString(L"suggestedFilename",
      webkit_glue::StringToStdWString(resource.suggested_filename));

  value->SetInteger(L"expectedContentLength",
                    resource.expected_content_length);
  value->SetInteger(L"responseStatusCode", resource.http_status_code);

  value->Set(L"requestHeaders",
             BuildValueForHeaders(resource.request_headers));
  value->Set(L"responseHeaders",
             BuildValueForHeaders(resource.response_headers));

  value->SetBoolean(L"isMainResource", resource.main_resource);
  value->SetBoolean(L"cached", false);

  if (resource.error_code) {
    value->SetInteger(L"errorCode", resource.error_code);
    value->SetString(L"localizedDescription",
        webkit_glue::StringToStdString(resource.error_description));
  }
}

void NetAgentImpl::ExpireFinishedResourcesCache() {
  if (finished_resources_.size() > 100) {
    // Preserve main resource.
    for (int i = 1; i < 21; ++i) {
      delete finished_resources_[i].second;
    }
    finished_resources_.remove(1, 21);
  }
}
