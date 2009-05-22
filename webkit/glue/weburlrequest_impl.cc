// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "FormData.h"
#include "HTTPHeaderMap.h"
#include "ResourceRequest.h"

#undef LOG
#include "base/logging.h"
#include "googleurl/src/gurl.h"
#include "net/base/upload_data.h"
#include "webkit/glue/weburlrequest_impl.h"
#include "webkit/glue/glue_serialize.h"
#include "webkit/glue/glue_util.h"

using WebCore::FormData;
using WebCore::FormDataElement;
using WebCore::HTTPHeaderMap;
using WebCore::ResourceRequest;
using WebCore::ResourceRequestCachePolicy;
using WebCore::String;

// WebRequest -----------------------------------------------------------------

WebRequestImpl::WebRequestImpl() {
}

WebRequestImpl::WebRequestImpl(const GURL& url)
    : request_(ResourceRequest(webkit_glue::GURLToKURL(url))) {
}

WebRequestImpl::WebRequestImpl(const ResourceRequest& request)
    : request_(request) {
}

WebRequest* WebRequestImpl::Clone() const {
  return new WebRequestImpl(*this);
}

GURL WebRequestImpl::GetURL() const {
  return webkit_glue::KURLToGURL(request_.url());
}

void WebRequestImpl::SetURL(const GURL& url) {
  request_.setURL(webkit_glue::GURLToKURL(url));
}

GURL WebRequestImpl::GetFirstPartyForCookies() const {
  return webkit_glue::KURLToGURL(
      request_.resourceRequest().firstPartyForCookies());
}

void WebRequestImpl::SetFirstPartyForCookies(const GURL& url) {
  request_.resourceRequest().setFirstPartyForCookies(
      webkit_glue::GURLToKURL(url));
}

WebRequestCachePolicy WebRequestImpl::GetCachePolicy() const {
  // WebRequestCachePolicy mirrors ResourceRequestCachePolicy
  return static_cast<WebRequestCachePolicy>(request_.cachePolicy());
}

void WebRequestImpl::SetCachePolicy(WebRequestCachePolicy policy) {
  // WebRequestCachePolicy mirrors ResourceRequestCachePolicy
  request_.setCachePolicy(
      static_cast<ResourceRequestCachePolicy>(policy));
}

std::string WebRequestImpl::GetHttpMethod() const {
  return webkit_glue::StringToStdString(request_.httpMethod());
}

void WebRequestImpl::SetHttpMethod(const std::string& method) {
  request_.setHTTPMethod(webkit_glue::StdStringToString(method));
}

std::string WebRequestImpl::GetHttpHeaderValue(const std::string& field) const {
  return webkit_glue::StringToStdString(
      request_.httpHeaderField(webkit_glue::StdStringToString(field)));
}

void WebRequestImpl::SetHttpHeaderValue(const std::string& field,
                                        const std::string& value) {
  request_.setHTTPHeaderField(
      webkit_glue::StdStringToString(field),
      webkit_glue::StdStringToString(value));
}

void WebRequestImpl::GetHttpHeaders(HeaderMap* headers) const {
  headers->clear();

  const HTTPHeaderMap& map = request_.httpHeaderFields();
  HTTPHeaderMap::const_iterator end = map.end();
  HTTPHeaderMap::const_iterator it = map.begin();
  for (; it != end; ++it) {
    headers->insert(std::make_pair(
        webkit_glue::StringToStdString(it->first),
        webkit_glue::StringToStdString(it->second)));
  }
}

void WebRequestImpl::SetHttpHeaders(const HeaderMap& headers) {
  HeaderMap::const_iterator end = headers.end();
  HeaderMap::const_iterator it = headers.begin();
  for (; it != end; ++it) {
    request_.setHTTPHeaderField(
        webkit_glue::StdStringToString(it->first),
        webkit_glue::StdStringToString(it->second));
  }
}

std::string WebRequestImpl::GetHttpReferrer() const {
  return webkit_glue::StringToStdString(request_.httpReferrer());
}

std::string WebRequestImpl::GetSecurityInfo() const {
  return webkit_glue::CStringToStdString(request_.securityInfo());
}

void WebRequestImpl::SetSecurityInfo(const std::string& value) {
  request_.setSecurityInfo(webkit_glue::StdStringToCString(value));
}

bool WebRequestImpl::HasUploadData() const {
  FormData* formdata = request_.httpBody();
  return formdata && !formdata->isEmpty();
}

void WebRequestImpl::GetUploadData(net::UploadData* data) const {
  FormData* formdata = request_.httpBody();
  if (!formdata)
    return;

  const Vector<FormDataElement>& elements = formdata->elements();
  Vector<FormDataElement>::const_iterator it = elements.begin();
  for (; it != elements.end(); ++it) {
    const FormDataElement& element = (*it);
    if (element.m_type == FormDataElement::data) {
      data->AppendBytes(element.m_data.data(), element.m_data.size());
    } else if (element.m_type == FormDataElement::encodedFile) {
      data->AppendFile(
          FilePath(webkit_glue::StringToFilePathString(element.m_filename)));
    } else {
      NOTREACHED();
    }
  }

  data->set_identifier(formdata->identifier());
}

void WebRequestImpl::SetUploadData(const net::UploadData& data)
{
  RefPtr<FormData> formdata = FormData::create();

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
          webkit_glue::FilePathStringToString(element.file_path().value()));
    } else {
      NOTREACHED();
    }
  }

  formdata->setIdentifier(data.identifier());

  request_.setHTTPBody(formdata);
}

void WebRequestImpl::SetRequestorID(int requestor_id) {
  request_.setRequestorID(requestor_id);
}

// static
WebRequest* WebRequest::Create(const GURL& url) {
  return new WebRequestImpl(url);
}
