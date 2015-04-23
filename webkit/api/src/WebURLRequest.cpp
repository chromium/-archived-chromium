/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebURLRequest.h"

#include "WebHTTPBody.h"
#include "WebHTTPHeaderVisitor.h"
#include "WebURL.h"
#include "WebURLRequestPrivate.h"

#include "ResourceRequest.h"

using namespace WebCore;

namespace WebKit {

// The standard implementation of WebURLRequestPrivate, which maintains
// ownership of a ResourceRequest instance.
class WebURLRequestPrivateImpl : public WebURLRequestPrivate {
public:
    WebURLRequestPrivateImpl()
    {
        m_resourceRequest = &m_resourceRequestAllocation;
    }

    WebURLRequestPrivateImpl(const WebURLRequestPrivate* p)
        : m_resourceRequestAllocation(*p->m_resourceRequest)
    {
        m_resourceRequest = &m_resourceRequestAllocation;
    }

    virtual void dispose() { delete this; }

    ResourceRequest m_resourceRequestAllocation;
};

void WebURLRequest::initialize()
{
    assign(new WebURLRequestPrivateImpl());
}

void WebURLRequest::reset()
{
    assign(0);
}

void WebURLRequest::assign(const WebURLRequest& r)
{
    assign(r.m_private ? new WebURLRequestPrivateImpl(r.m_private) : 0);
}

WebURL WebURLRequest::url() const
{
    return m_private->m_resourceRequest->url();
}

void WebURLRequest::setURL(const WebURL& url)
{
    m_private->m_resourceRequest->setURL(url);
}

WebURL WebURLRequest::firstPartyForCookies() const
{
    return m_private->m_resourceRequest->firstPartyForCookies();
}

void WebURLRequest::setFirstPartyForCookies(const WebURL& firstPartyForCookies)
{
    m_private->m_resourceRequest->setFirstPartyForCookies(firstPartyForCookies);
}

WebURLRequest::CachePolicy WebURLRequest::cachePolicy() const
{
    return static_cast<WebURLRequest::CachePolicy>(
        m_private->m_resourceRequest->cachePolicy());
}

void WebURLRequest::setCachePolicy(CachePolicy cachePolicy)
{
    m_private->m_resourceRequest->setCachePolicy(
        static_cast<ResourceRequestCachePolicy>(cachePolicy));
}

WebString WebURLRequest::httpMethod() const
{
    return m_private->m_resourceRequest->httpMethod();
}

void WebURLRequest::setHTTPMethod(const WebString& httpMethod)
{
    m_private->m_resourceRequest->setHTTPMethod(httpMethod);
}

WebString WebURLRequest::httpHeaderField(const WebString& name) const
{
    return m_private->m_resourceRequest->httpHeaderField(String(name));
}

void WebURLRequest::setHTTPHeaderField(const WebString& name, const WebString& value)
{
    m_private->m_resourceRequest->setHTTPHeaderField(String(name), value);
}

void WebURLRequest::addHTTPHeaderField(const WebString& name, const WebString& value)
{
    m_private->m_resourceRequest->addHTTPHeaderField(String(name), value);
}

void WebURLRequest::clearHTTPHeaderField(const WebString& name)
{
    // FIXME: Add a clearHTTPHeaderField method to ResourceRequest.
    const HTTPHeaderMap& map = m_private->m_resourceRequest->httpHeaderFields();
    const_cast<HTTPHeaderMap*>(&map)->remove(String(name));
}

void WebURLRequest::visitHTTPHeaderFields(WebHTTPHeaderVisitor* visitor) const
{
    const HTTPHeaderMap& map = m_private->m_resourceRequest->httpHeaderFields();
    for (HTTPHeaderMap::const_iterator it = map.begin(); it != map.end(); ++it)
        visitor->visitHeader(String(it->first), it->second);
}

WebHTTPBody WebURLRequest::httpBody() const
{
    return WebHTTPBody(m_private->m_resourceRequest->httpBody());
}

void WebURLRequest::setHTTPBody(const WebHTTPBody& httpBody)
{
    m_private->m_resourceRequest->setHTTPBody(httpBody);
}

bool WebURLRequest::reportUploadProgress() const
{
    return m_private->m_resourceRequest->reportUploadProgress();
}

void WebURLRequest::setReportUploadProgress(bool reportUploadProgress)
{
    m_private->m_resourceRequest->setReportUploadProgress(reportUploadProgress);
}

WebURLRequest::TargetType WebURLRequest::targetType() const
{
    return static_cast<TargetType>(m_private->m_resourceRequest->targetType());
}

void WebURLRequest::setTargetType(TargetType targetType)
{
    m_private->m_resourceRequest->setTargetType(
        static_cast<ResourceRequest::TargetType>(targetType));
}

int WebURLRequest::requestorID() const
{
    return m_private->m_resourceRequest->requestorID();
}

void WebURLRequest::setRequestorID(int requestorID)
{
    m_private->m_resourceRequest->setRequestorID(requestorID);
}

int WebURLRequest::requestorProcessID() const
{
    return m_private->m_resourceRequest->requestorProcessID();
}

void WebURLRequest::setRequestorProcessID(int requestorProcessID)
{
    m_private->m_resourceRequest->setRequestorProcessID(requestorProcessID);
}

int WebURLRequest::appCacheContextID() const
{
    return m_private->m_resourceRequest->appCacheContextID();
}

void WebURLRequest::setAppCacheContextID(int appCacheContextID)
{
    m_private->m_resourceRequest->setAppCacheContextID(appCacheContextID);
}

ResourceRequest& WebURLRequest::toMutableResourceRequest()
{
    ASSERT(m_private);
    ASSERT(m_private->m_resourceRequest);

    return *m_private->m_resourceRequest;
}

const ResourceRequest& WebURLRequest::toResourceRequest() const
{
    ASSERT(m_private);
    ASSERT(m_private->m_resourceRequest);

    return *m_private->m_resourceRequest;
}

void WebURLRequest::assign(WebURLRequestPrivate* p)
{
    if (m_private == p)
        return;
    if (m_private)
        m_private->dispose();
    m_private = p;
}

} // namespace WebKit
