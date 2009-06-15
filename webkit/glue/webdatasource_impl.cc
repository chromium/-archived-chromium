// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "webkit/glue/webdatasource_impl.h"

#include "FrameLoaderTypes.h"
#include "FrameLoadRequest.h"
#include "KURL.h"
#include "ResourceRequest.h"

#undef LOG
#include "base/histogram.h"
#include "base/string_util.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/password_form.h"
#include "webkit/glue/webdatasource_impl.h"
#include "webkit/glue/webframe_impl.h"
#include "webkit/glue/weburlrequest_impl.h"
#include "webkit/glue/webview_delegate.h"

using base::TimeDelta;
using base::Time;

// static
PassRefPtr<WebDataSourceImpl> WebDataSourceImpl::Create(
    const WebCore::ResourceRequest& request,
    const WebCore::SubstituteData& data) {
  return adoptRef(new WebDataSourceImpl(request, data));
}

WebDataSourceImpl::WebDataSourceImpl(const WebCore::ResourceRequest& request,
                                     const WebCore::SubstituteData& data)
    : WebCore::DocumentLoader(request, data) {
}

WebDataSourceImpl::~WebDataSourceImpl() {
}

const WebRequest& WebDataSourceImpl::GetInitialRequest() const {
  // WebKit may change the frame load request as it sees fit, so we must sync
  // our request object.
  initial_request_.set_resource_request(originalRequest());
  return initial_request_;
}

const WebRequest& WebDataSourceImpl::GetRequest() const {
  // WebKit may change the frame load request as it sees fit, so we must sync
  // our request object.
  request_.set_resource_request(request());
  return request_;
}

const WebResponse& WebDataSourceImpl::GetResponse() const {
  response_.set_resource_response(response());
  return response_;
}

GURL WebDataSourceImpl::GetUnreachableURL() const {
  const WebCore::KURL& url = unreachableURL();
  return url.isEmpty() ? GURL() : webkit_glue::KURLToGURL(url);
}

bool WebDataSourceImpl::HasUnreachableURL() const {
  return !unreachableURL().isEmpty();
}

const std::vector<GURL>& WebDataSourceImpl::GetRedirectChain() const {
  return redirect_chain_;
}

void WebDataSourceImpl::ClearRedirectChain() {
  redirect_chain_.clear();
}

void WebDataSourceImpl::AppendRedirect(const GURL& url) {
  redirect_chain_.push_back(url);
}

string16 WebDataSourceImpl::GetPageTitle() const {
  return webkit_glue::StringToString16(title());
}

base::Time WebDataSourceImpl::GetRequestTime() const {
  return request_time_;
}

void WebDataSourceImpl::SetRequestTime(base::Time time) {
  request_time_ = time;
}

base::Time WebDataSourceImpl::GetStartLoadTime() const {
  return start_load_time_;
}

base::Time WebDataSourceImpl::GetFinishDocumentLoadTime() const {
  return finish_document_load_time_;
}

base::Time WebDataSourceImpl::GetFinishLoadTime() const {
  return finish_load_time_;
}

base::Time WebDataSourceImpl::GetFirstLayoutTime() const {
  return first_layout_time_;
}

WebNavigationType WebDataSourceImpl::GetNavigationType() const {
  return NavigationTypeToWebNavigationType(triggeringAction().type());
}

WebDataSource::ExtraData* WebDataSourceImpl::GetExtraData() const {
  return extra_data_.get();
}

void WebDataSourceImpl::SetExtraData(ExtraData* extra_data) {
  extra_data_.set(extra_data);
}

WebNavigationType WebDataSourceImpl::NavigationTypeToWebNavigationType(
    WebCore::NavigationType type) {
  switch (type) {
    case WebCore::NavigationTypeLinkClicked:
      return WebNavigationTypeLinkClicked;
    case WebCore::NavigationTypeFormSubmitted:
      return WebNavigationTypeFormSubmitted;
    case WebCore::NavigationTypeBackForward:
      return WebNavigationTypeBackForward;
    case WebCore::NavigationTypeReload:
      return WebNavigationTypeReload;
    case WebCore::NavigationTypeFormResubmitted:
      return WebNavigationTypeFormResubmitted;
    case WebCore::NavigationTypeOther:
    default:
      return WebNavigationTypeOther;
  }
}
