// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "config.h"
#include "webkit/glue/weburlrequest_impl.h"
#include "webkit/glue/glue_serialize.h"
#include "webkit/glue/glue_util.h"

using WebCore::FrameLoadRequest;
using WebCore::ResourceRequest;
using WebCore::ResourceRequestCachePolicy;
using WebCore::String;

// WebRequest -----------------------------------------------------------------

WebRequestImpl::WebRequestImpl() {
}

WebRequestImpl::WebRequestImpl(const GURL& url)
    : request_(FrameLoadRequest(webkit_glue::GURLToKURL(url))) {
}

WebRequestImpl::WebRequestImpl(const WebCore::ResourceRequest& request)
    : request_(FrameLoadRequest(request)) {
}

WebRequestImpl::WebRequestImpl(const WebCore::FrameLoadRequest& request)
    : request_(request) {
}

WebRequest* WebRequestImpl::Clone() const {
  return new WebRequestImpl(*this);
}

void WebRequestImpl::SetExtraData(ExtraData* extra) {
  extra_data_ = extra;
}

WebRequest::ExtraData* WebRequestImpl::GetExtraData() const {
  return extra_data_.get();
}

GURL WebRequestImpl::GetURL() const {
  return webkit_glue::KURLToGURL(request_.resourceRequest().url());
}

void WebRequestImpl::SetURL(const GURL& url) {
  request_.resourceRequest().setURL(webkit_glue::GURLToKURL(url));
}

GURL WebRequestImpl::GetMainDocumentURL() const {
  return webkit_glue::KURLToGURL(request_.resourceRequest().mainDocumentURL());
}

void WebRequestImpl::SetMainDocumentURL(const GURL& url) {
  request_.resourceRequest().setMainDocumentURL(webkit_glue::GURLToKURL(url));
}

WebRequestCachePolicy WebRequestImpl::GetCachePolicy() const {
  // WebRequestCachePolicy mirrors ResourceRequestCachePolicy
  return static_cast<WebRequestCachePolicy>(
      request_.resourceRequest().cachePolicy());
}

void WebRequestImpl::SetCachePolicy(WebRequestCachePolicy policy) {
  // WebRequestCachePolicy mirrors ResourceRequestCachePolicy
  request_.resourceRequest().setCachePolicy(
      static_cast<WebCore::ResourceRequestCachePolicy>(policy));
}

std::wstring WebRequestImpl::GetHttpMethod() const {
  return webkit_glue::StringToStdWString(
      request_.resourceRequest().httpMethod());
}

void WebRequestImpl::SetHttpMethod(const std::wstring& method) {
  request_.resourceRequest().setHTTPMethod(
      webkit_glue::StdWStringToString(method));
}

std::wstring WebRequestImpl::GetHttpHeaderValue(const std::wstring& field) const {
  return webkit_glue::StringToStdWString(
      request_.resourceRequest().httpHeaderField(
          webkit_glue::StdWStringToString(field)));
}

std::wstring WebRequestImpl::GetHttpReferrer() const {
  return webkit_glue::StringToStdWString(
      request_.resourceRequest().httpReferrer());
}

std::string WebRequestImpl::GetHistoryState() const {
  std::string value;
  webkit_glue::HistoryItemToString(history_item_, &value);
  return value;
}

void WebRequestImpl::SetHistoryState(const std::string& value) {
  history_item_ = webkit_glue::HistoryItemFromString(value);
}

std::string WebRequestImpl::GetSecurityInfo() const {
  return webkit_glue::CStringToStdString(
      request_.resourceRequest().securityInfo());
}

void WebRequestImpl::SetSecurityInfo(const std::string& value) {
  request_.resourceRequest().setSecurityInfo(
      webkit_glue::StdStringToCString(value));
}

bool WebRequestImpl::HasFormData() const {
  return history_item() && history_item()->formData();
}

// static
WebRequest* WebRequest::Create(const GURL& url) {
  return new WebRequestImpl(url);
}
