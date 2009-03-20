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
  DictionaryValue value;
  value.SetReal(L"startTime", WTF::currentTime());
  value.SetString(L"url",  webkit_glue::StringToStdString(url.string()));
  value.SetString(L"domain", webkit_glue::StringToStdString(url.host()));
  value.SetString(L"path", webkit_glue::StringToStdString(url.path()));
  value.SetString(L"lastPathComponent",
      webkit_glue::StringToStdString(url.lastPathComponent()));
  value.Set(L"requestHeaders",
      BuildValueForHeaders(request.httpHeaderFields()));
  delegate_->WillSendRequest(identifier, value);
}

void NetAgentImpl::DidReceiveResponse(
    DocumentLoader* loader,
    int identifier,
    const ResourceResponse &response) {
  if (!document_) {
    return;
  }
  KURL url = response.url();

  DictionaryValue value;
  value.SetReal(L"responseReceivedTime", WTF::currentTime());
  value.SetString(L"url",
      webkit_glue::StringToStdWString(url.string()));
  value.SetInteger(L"expectedContentLength",
      static_cast<int>(response.expectedContentLength()));
  value.SetInteger(L"responseStatusCode", response.httpStatusCode());
  value.SetString(L"mimeType",
      webkit_glue::StringToStdWString(response.mimeType()));
  value.SetString(L"suggestedFilename",
      webkit_glue::StringToStdWString(response.suggestedFilename()));
  value.Set(L"responseHeaders",
      BuildValueForHeaders(response.httpHeaderFields()));

  delegate_->DidReceiveResponse(identifier, value);
}

void NetAgentImpl::DidReceiveContentLength(
    DocumentLoader* loader,
    int identifier,
    int length) {
}

void NetAgentImpl::DidFinishLoading(
    DocumentLoader* loader,
    int identifier) {
  DictionaryValue value;
  value.SetReal(L"endTime", WTF::currentTime());
  delegate_->DidFinishLoading(identifier, value);
}

void NetAgentImpl::DidFailLoading(
    DocumentLoader* loader,
    int identifier,
    const ResourceError& error) {
  DictionaryValue value;
  value.SetReal(L"endTime", WTF::currentTime());
  value.SetInteger(L"errorCode", error.errorCode());
  value.SetString(L"localizedDescription",
      webkit_glue::StringToStdString(error.localizedDescription()));
  delegate_->DidFailLoading(identifier, value);
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
  HashMap<int, RefPtr<DocumentLoader> >::iterator it =
      loaders_.find(identifier);
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
