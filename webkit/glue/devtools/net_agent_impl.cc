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
#include "HTTPHeaderMap.h"
#include "KURL.h"
#include "PlatformString.h"
#include "ResourceError.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
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
      last_cached_identifier_(-2) {
}

NetAgentImpl::~NetAgentImpl() {
  SetDocument(NULL);
  for (CachedResources::iterator  it = pending_resources_.begin();
       it != pending_resources_.end(); ++it) {
    delete it->second;
  }
  pending_resources_.clear();
}

void NetAgentImpl::SetDocument(Document* doc) {
//  loaders_.clear();
  document_ = doc;
}

void NetAgentImpl::AssignIdentifierToRequest(
    DocumentLoader* loader,
    int identifier,
    const ResourceRequest& request) {
  loaders_.set(identifier, loader);
}

void NetAgentImpl::WillSendRequest(
    DocumentLoader* loader,
    int identifier,
    const ResourceRequest& request) {
  KURL url = request.url();
  DictionaryValue* resource = new DictionaryValue();

  resource->SetReal(L"startTime", WTF::currentTime());
  resource->SetString(L"url",  webkit_glue::StringToStdString(url.string()));
  resource->SetString(L"domain", webkit_glue::StringToStdString(url.host()));
  resource->SetString(L"path", webkit_glue::StringToStdString(url.path()));
  resource->SetString(L"lastPathComponent",
      webkit_glue::StringToStdString(url.lastPathComponent()));
  resource->Set(L"requestHeaders",
      BuildValueForHeaders(request.httpHeaderFields()));
  delegate_->WillSendRequest(identifier, *resource);
  pending_resources_.set(identifier, resource);
}

void NetAgentImpl::DidReceiveResponse(
    DocumentLoader* loader,
    int identifier,
    const ResourceResponse &response) {
  KURL url = response.url();
  if (!pending_resources_.contains(identifier)) {
    return;
  }
  DictionaryValue* resource = pending_resources_.get(identifier);
  resource->SetReal(L"responseReceivedTime", WTF::currentTime());
  resource->SetString(L"url",
      webkit_glue::StringToStdWString(url.string()));
  resource->SetInteger(L"expectedContentLength",
      static_cast<int>(response.expectedContentLength()));
  resource->SetInteger(L"responseStatusCode", response.httpStatusCode());
  resource->SetString(L"mimeType",
      webkit_glue::StringToStdWString(response.mimeType()));
  resource->SetString(L"suggestedFilename",
      webkit_glue::StringToStdWString(response.suggestedFilename()));
  resource->Set(L"responseHeaders",
      BuildValueForHeaders(response.httpHeaderFields()));

  delegate_->DidReceiveResponse(identifier, *resource);
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
  DictionaryValue* resource = pending_resources_.get(identifier);
  resource->SetReal(L"endTime", WTF::currentTime());
  delegate_->DidFinishLoading(identifier, *resource);
  pending_resources_.remove(identifier);
  delete resource;
}

void NetAgentImpl::DidFailLoading(
    DocumentLoader* loader,
    int identifier,
    const ResourceError& error) {
  if (!pending_resources_.contains(identifier)) {
    return;
  }
  DictionaryValue* resource = pending_resources_.get(identifier);
  resource->SetReal(L"endTime", WTF::currentTime());
  resource->SetInteger(L"errorCode", error.errorCode());
  resource->SetString(L"localizedDescription",
      webkit_glue::StringToStdString(error.localizedDescription()));
  delegate_->DidFailLoading(identifier, *resource);
  pending_resources_.remove(identifier);
  delete resource;
}

void NetAgentImpl::DidLoadResourceFromMemoryCache(
    DocumentLoader* loader,
    const ResourceRequest& request,
    const ResourceResponse& response,
    int length) {
  int identifier = last_cached_identifier_--;
  loaders_.set(identifier, loader);
}

void NetAgentImpl::GetResourceContent(
    int call_id,
    int identifier,
    const String& url) {
  if (!document_) {
    return;
  }
  CachedLoaders::iterator it = loaders_.find(identifier);
  if (it == loaders_.end() || !it->second) {
    return;
  }

  RefPtr<DocumentLoader> loader = it->second;
  String source;

  if (url == loader->requestURL()) {
    RefPtr<SharedBuffer> buffer = loader->mainResourceData();
    String text_encoding_name = document_->inputEncoding();
    if (buffer) {
      WebCore::TextEncoding encoding(text_encoding_name);
      if (!encoding.isValid())
        encoding = WindowsLatin1Encoding();
      source = encoding.decode(buffer->data(), buffer->size());
    }
  } else {
    CachedResource* cachedResource = document_->
        docLoader()->cachedResource(url);
    if (!cachedResource->isPurgeable()) {
      // TODO(pfeldman): Try making unpurgeable.
      return;
    }

    // Try to get the decoded source.  Only applies to some CachedResource
    // types.
    switch (cachedResource->type()) {
      case CachedResource::CSSStyleSheet: {
        CachedCSSStyleSheet *sheet =
            reinterpret_cast<CachedCSSStyleSheet*>(cachedResource);
        source = sheet->sheetText();
        break;
      }
      case CachedResource::Script: {
        CachedScript *script =
            reinterpret_cast<CachedScript*>(cachedResource);
        source = script->script();
        break;
      }
#if ENABLE(XSLT)
      case CachedResource::XSLStyleSheet: {
        CachedXSLStyleSheet *sheet =
            reinterpret_cast<CachedXSLStyleSheet*>(cachedResource);
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

Value* NetAgentImpl::BuildValueForHeaders(const HTTPHeaderMap& headers) {
  OwnPtr<DictionaryValue> value(new DictionaryValue());
  HTTPHeaderMap::const_iterator end = headers.end();
  for (HTTPHeaderMap::const_iterator it = headers.begin(); it != end; ++it) {
    value->SetString(webkit_glue::StringToStdWString(it->first),
        webkit_glue::StringToStdString(it->second));
  }
  return value.release();
}
