// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "base/compiler_specific.h"

MSVC_PUSH_WARNING_LEVEL(0);
#include "FormData.h"
#include "HTTPHeaderMap.h"
#include "ResourceRequest.h"
MSVC_POP_WARNING();

#undef LOG
#include "base/logging.h"
#include "googleurl/src/gurl.h"
#include "net/base/upload_data.h"
#include "webkit/glue/weburlrequest_impl.h"
#include "webkit/glue/glue_serialize.h"
#include "webkit/glue/glue_util.h"

using WebCore::FrameLoadRequest;
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

std::string WebRequestImpl::GetHttpMethod() const {
  return webkit_glue::StringToStdString(
      request_.resourceRequest().httpMethod());
}

void WebRequestImpl::SetHttpMethod(const std::string& method) {
  request_.resourceRequest().setHTTPMethod(
      webkit_glue::StdStringToString(method));
}

std::string WebRequestImpl::GetHttpHeaderValue(const std::string& field) const {
  return webkit_glue::StringToStdString(
      request_.resourceRequest().httpHeaderField(
          webkit_glue::StdStringToString(field)));
}

void WebRequestImpl::SetHttpHeaderValue(const std::string& field,
                                        const std::string& value) {
  request_.resourceRequest().setHTTPHeaderField(
      webkit_glue::StdStringToString(field),
      webkit_glue::StdStringToString(value));
}

void WebRequestImpl::GetHttpHeaders(HeaderMap* headers) const {
  headers->clear();

  const WebCore::HTTPHeaderMap& map =
      request_.resourceRequest().httpHeaderFields();
  WebCore::HTTPHeaderMap::const_iterator end = map.end();
  WebCore::HTTPHeaderMap::const_iterator it = map.begin();
  for (; it != end; ++it) {
    headers->insert(
        std::make_pair(
            webkit_glue::StringToStdString(it->first),
            webkit_glue::StringToStdString(it->second)));
  }
}

void WebRequestImpl::SetHttpHeaders(const HeaderMap& headers) {
  WebCore::ResourceRequest& request = request_.resourceRequest();

  HeaderMap::const_iterator end = headers.end();
  HeaderMap::const_iterator it = headers.begin();
  for (; it != end; ++it) {
    request.setHTTPHeaderField(
        webkit_glue::StdStringToString(it->first),
        webkit_glue::StdStringToString(it->second));
  }
}

std::string WebRequestImpl::GetHttpReferrer() const {
  return webkit_glue::StringToStdString(
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

bool WebRequestImpl::HasUploadData() const {
  WebCore::FormData* formdata = request_.resourceRequest().httpBody();
  return formdata && !formdata->isEmpty();
}

void WebRequestImpl::GetUploadData(net::UploadData* data) const {
  WebCore::FormData* formdata = request_.resourceRequest().httpBody();
  if (!formdata)
    return;

  const Vector<WebCore::FormDataElement>& elements = formdata->elements();
  Vector<WebCore::FormDataElement>::const_iterator it = elements.begin();
  for (; it != elements.end(); ++it) {
    const WebCore::FormDataElement& element = (*it);
    if (element.m_type == WebCore::FormDataElement::data) {
      data->AppendBytes(element.m_data.data(), element.m_data.size());
    } else if (element.m_type == WebCore::FormDataElement::encodedFile) {
      data->AppendFile(webkit_glue::StringToStdWString(element.m_filename));
    } else {
      NOTREACHED();
    }
  }
}

void WebRequestImpl::SetUploadData(const net::UploadData& data)
{
  RefPtr<WebCore::FormData> formdata = WebCore::FormData::create();

  const std::vector<net::UploadData::Element>& elements = data.elements();
  std::vector<net::UploadData::Element>::const_iterator it = elements.begin();
  for (; it != elements.end(); ++it) {
    const net::UploadData::Element& element = (*it);
    if (element.type() == net::UploadData::TYPE_BYTES) {
      formdata->appendData(
          std::string(element.bytes().begin(), element.bytes().end()).c_str(),
          element.bytes().size());
    } else if (element.type() == net::UploadData::TYPE_FILE) {
      formdata->appendFile(
          webkit_glue::StdWStringToString(element.file_path()));
    } else {
      NOTREACHED();
    }
  }

  request_.resourceRequest().setHTTPBody(formdata);
}

// static
WebRequest* WebRequest::Create(const GURL& url) {
  return new WebRequestImpl(url);
}
