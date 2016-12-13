// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "webkit/glue/webdatasource_impl.h"

#include "webkit/api/public/WebURL.h"
#include "webkit/api/public/WebVector.h"
#include "webkit/glue/glue_util.h"

using WebCore::DocumentLoader;
using WebCore::ResourceRequest;
using WebCore::ResourceResponse;
using WebCore::SubstituteData;

using WebKit::WebDataSource;
using WebKit::WebNavigationType;
using WebKit::WebString;
using WebKit::WebURL;
using WebKit::WebURLRequest;
using WebKit::WebURLResponse;
using WebKit::WebVector;

// static
PassRefPtr<WebDataSourceImpl> WebDataSourceImpl::Create(
    const ResourceRequest& request,
    const SubstituteData& data) {
  return adoptRef(new WebDataSourceImpl(request, data));
}

WebDataSourceImpl::WebDataSourceImpl(const ResourceRequest& request,
                                     const SubstituteData& data)
    : DocumentLoader(request, data) {
}

WebDataSourceImpl::~WebDataSourceImpl() {
}

const WebURLRequest& WebDataSourceImpl::originalRequest() const {
  original_request_.bind(DocumentLoader::originalRequest());
  return original_request_;
}

const WebURLRequest& WebDataSourceImpl::request() const {
  request_.bind(DocumentLoader::request());
  return request_;
}

const WebURLResponse& WebDataSourceImpl::response() const {
  response_.bind(DocumentLoader::response());
  return response_;
}

bool WebDataSourceImpl::hasUnreachableURL() const {
  return !DocumentLoader::unreachableURL().isEmpty();
}

WebURL WebDataSourceImpl::unreachableURL() const {
  return webkit_glue::KURLToWebURL(DocumentLoader::unreachableURL());
}

void WebDataSourceImpl::redirectChain(WebVector<WebURL>& result) const {
  result.assign(redirect_chain_);
}

WebString WebDataSourceImpl::pageTitle() const {
  return webkit_glue::StringToWebString(title());
}

WebNavigationType WebDataSourceImpl::navigationType() const {
  return NavigationTypeToWebNavigationType(triggeringAction().type());
}

double WebDataSourceImpl::triggeringEventTime() const {
  if (!triggeringAction().event())
    return 0.0;

  // DOMTimeStamp uses units of milliseconds.
  return triggeringAction().event()->timeStamp() / 1000.0;
}

WebDataSource::ExtraData* WebDataSourceImpl::extraData() const {
  return extra_data_.get();
}

void WebDataSourceImpl::setExtraData(ExtraData* extra_data) {
  extra_data_.set(extra_data);
}

WebNavigationType WebDataSourceImpl::NavigationTypeToWebNavigationType(
    WebCore::NavigationType type) {
  switch (type) {
    case WebCore::NavigationTypeLinkClicked:
      return WebKit::WebNavigationTypeLinkClicked;
    case WebCore::NavigationTypeFormSubmitted:
      return WebKit::WebNavigationTypeFormSubmitted;
    case WebCore::NavigationTypeBackForward:
      return WebKit::WebNavigationTypeBackForward;
    case WebCore::NavigationTypeReload:
      return WebKit::WebNavigationTypeReload;
    case WebCore::NavigationTypeFormResubmitted:
      return WebKit::WebNavigationTypeFormResubmitted;
    case WebCore::NavigationTypeOther:
    default:
      return WebKit::WebNavigationTypeOther;
  }
}

GURL WebDataSourceImpl::GetEndOfRedirectChain() const {
  ASSERT(!redirect_chain_.isEmpty());
  return redirect_chain_.last();
}

void WebDataSourceImpl::ClearRedirectChain() {
  redirect_chain_.clear();
}

void WebDataSourceImpl::AppendRedirect(const GURL& url) {
  redirect_chain_.append(url);
}
