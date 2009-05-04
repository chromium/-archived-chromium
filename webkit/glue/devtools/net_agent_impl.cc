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
    delegate_->DidFinishLoading(it->first, *it->second);
  }
  attached_ = true;
}

void NetAgentImpl::Detach() {
  attached_ = false;
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
  pending_resources_.set(identifier, resource);

  if (attached_) {
    delegate_->WillSendRequest(identifier, *resource);
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

  if (attached_) {
    delegate_->DidReceiveResponse(identifier, *resource);
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

  // This is the first command being dispatched after
  // DidCommitMainResourceLoad, we know that the first resource to be reported
  // as loaded is main resource.
  if (!main_loader_.get()) {
    main_loader_ = loader;
  }

  DictionaryValue* resource = pending_resources_.get(identifier);
  resource->SetReal(L"endTime", WTF::currentTime());

  pending_resources_.remove(identifier);
  finished_resources_.append(std::make_pair(identifier, resource));

  // Start removing resources from the cache once there are too many of them.
  if (finished_resources_.size() > 200) {
    for (int i = 0; i < 50; ++i) {
      xml_http_sources_.remove(finished_resources_[i].first);
      delete finished_resources_[i].second;
    }
    finished_resources_.remove(0, 50);
  }

  if (attached_) {
    delegate_->DidFinishLoading(identifier, *resource);
  }
}

void NetAgentImpl::DidFailLoading(
    DocumentLoader* loader,
    int identifier,
    const ResourceError& error) {
  if (!pending_resources_.contains(identifier)) {
    return;
  }
  DictionaryValue* resource = pending_resources_.get(identifier);
  resource->SetInteger(L"errorCode", error.errorCode());
  resource->SetString(L"localizedDescription",
      webkit_glue::StringToStdString(error.localizedDescription()));
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
  xml_http_sources_.set(identifier, source);
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

Value* NetAgentImpl::BuildValueForHeaders(const HTTPHeaderMap& headers) {
  OwnPtr<DictionaryValue> value(new DictionaryValue());
  HTTPHeaderMap::const_iterator end = headers.end();
  for (HTTPHeaderMap::const_iterator it = headers.begin(); it != end; ++it) {
    value->SetString(webkit_glue::StringToStdWString(it->first),
        webkit_glue::StringToStdString(it->second));
  }
  return value.release();
}
