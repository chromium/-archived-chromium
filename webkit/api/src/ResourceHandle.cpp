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
#include "ResourceHandle.h"

#include "WebKit.h"
#include "WebKitClient.h"
#include "WebURLError.h"
#include "WebURLLoader.h"
#include "WebURLLoaderClient.h"
#include "WebURLRequest.h"
#include "WebURLResponse.h"
#include "WrappedResourceRequest.h"
#include "WrappedResourceResponse.h"

#include "ResourceHandleClient.h"
#include "ResourceRequest.h"

using namespace WebKit;

namespace WebCore {

// ResourceHandleInternal -----------------------------------------------------

class ResourceHandleInternal : public WebURLLoaderClient {
public:
    ResourceHandleInternal(const ResourceRequest& request, ResourceHandleClient* client)
        : m_request(request)
        , m_owner(0)
        , m_client(client)
    {
    }

    void start();
    void cancel();
    void setDefersLoading(bool);

    // WebURLLoaderClient methods:
    virtual void willSendRequest(WebURLLoader*, WebURLRequest&, const WebURLResponse&);
    virtual void didSendData(
        WebURLLoader*, unsigned long long bytesSent, unsigned long long totalBytesToBeSent);
    virtual void didReceiveResponse(WebURLLoader*, const WebURLResponse&);
    virtual void didReceiveData(
        WebURLLoader*, const char* data, int dataLength, long long totalDataLength);
    virtual void didFinishLoading(WebURLLoader*);
    virtual void didFail(WebURLLoader*, const WebURLError&);

    ResourceRequest m_request;
    ResourceHandle* m_owner;
    ResourceHandleClient* m_client;
    OwnPtr<WebURLLoader> m_loader;
};

void ResourceHandleInternal::start()
{
    m_loader.set(webKitClient()->createURLLoader());
    ASSERT(m_loader.get());

    WrappedResourceRequest wrappedRequest(m_request);
    m_loader->loadAsynchronously(wrappedRequest, this);
}

void ResourceHandleInternal::cancel()
{
    m_loader->cancel();

    // Do not make any further calls to the client.
    m_client = 0;
}

void ResourceHandleInternal::setDefersLoading(bool value)
{
    m_loader->setDefersLoading(value);
}

void ResourceHandleInternal::willSendRequest(
    WebURLLoader*, WebURLRequest& request, const WebURLResponse& response)
{
    ASSERT(m_client);
    ASSERT(!request.isNull());
    ASSERT(!response.isNull());
    m_client->willSendRequest(m_owner, request.toMutableResourceRequest(), response.toResourceResponse());
}

void ResourceHandleInternal::didSendData(
    WebURLLoader*, unsigned long long bytesSent, unsigned long long totalBytesToBeSent)
{
    ASSERT(m_client);
    m_client->didSendData(m_owner, bytesSent, totalBytesToBeSent);
}

void ResourceHandleInternal::didReceiveResponse(WebURLLoader*, const WebURLResponse& response)
{
    ASSERT(m_client);
    ASSERT(!response.isNull());
    m_client->didReceiveResponse(m_owner, response.toResourceResponse());
}

void ResourceHandleInternal::didReceiveData(
    WebURLLoader*, const char* data, int dataLength, long long totalDataLength)
{
    ASSERT(m_client);

    // FIXME: ResourceHandleClient::didReceiveData should take a 'long long'
    int lengthReceived = static_cast<int>(totalDataLength);
    if (lengthReceived != totalDataLength)  // overflow occurred
      lengthReceived = -1;

    m_client->didReceiveData(m_owner, data, dataLength, lengthReceived);
}

void ResourceHandleInternal::didFinishLoading(WebURLLoader*)
{
    ASSERT(m_client);
    m_client->didFinishLoading(m_owner);
}

void ResourceHandleInternal::didFail(WebURLLoader*, const WebURLError& error)
{
    ASSERT(m_client);
    m_client->didFail(m_owner, error);
}

// ResourceHandle -------------------------------------------------------------

ResourceHandle::ResourceHandle(const ResourceRequest& request,
                               ResourceHandleClient* client,
                               bool defersLoading,
                               bool shouldContentSniff,
                               bool mightDownloadFromHandle)
    : d(new ResourceHandleInternal(request, client))
{
    d->m_owner = this;

    // FIXME: Figure out what to do with the bool params.
}

PassRefPtr<ResourceHandle> ResourceHandle::create(const ResourceRequest& request,
                                                  ResourceHandleClient* client,
                                                  Frame* deprecated,
                                                  bool defersLoading,
                                                  bool shouldContentSniff,
                                                  bool mightDownloadFromHandle)
{
    RefPtr<ResourceHandle> newHandle = adoptRef(new ResourceHandle(
        request, client, defersLoading, shouldContentSniff, mightDownloadFromHandle));

    if (newHandle->start(deprecated))
        return newHandle.release();

    return 0;
}

const ResourceRequest& ResourceHandle::request() const
{
    return d->m_request;
}

ResourceHandleClient* ResourceHandle::client() const
{
    return d->m_client;
}

void ResourceHandle::setClient(ResourceHandleClient* client)
{
    d->m_client = client;
}

void ResourceHandle::setDefersLoading(bool value)
{
    d->setDefersLoading(value);
}

bool ResourceHandle::start(Frame* deprecated)
{
    d->start();
    return true;
}

void ResourceHandle::clearAuthentication()
{
}

void ResourceHandle::cancel()
{
    d->cancel();
}

ResourceHandle::~ResourceHandle()
{
    d->m_owner = 0;
}

PassRefPtr<SharedBuffer> ResourceHandle::bufferedData()
{
    return 0;
}

bool ResourceHandle::loadsBlocked()
{
    return false;  // This seems to be related to sync XMLHttpRequest...
}

// static
bool ResourceHandle::supportsBufferedData()
{
    return false;  // The loader will buffer manually if it needs to.
}

// static
void ResourceHandle::loadResourceSynchronously(const ResourceRequest& request,
                                               StoredCredentials unused,
                                               ResourceError& error,
                                               ResourceResponse& response,
                                               Vector<char>& data,
                                               Frame* deprecated)
{
    OwnPtr<WebURLLoader> loader(webKitClient()->createURLLoader());
    ASSERT(loader.get());

    WrappedResourceRequest requestIn(request);
    WrappedResourceResponse responseOut(response);
    WebURLError errorOut;
    WebData dataOut;

    loader->loadSynchronously(requestIn, responseOut, errorOut, dataOut);

    error = errorOut;
    data.clear();
    data.append(dataOut.data(), dataOut.size());
}

// static
bool ResourceHandle::willLoadFromCache(ResourceRequest& request)
{
    // This method is used to determine if a POST request can be repeated from
    // cache, but you cannot really know until you actually try to read from the
    // cache.  Even if we checked now, something else could come along and wipe
    // out the cache entry by the time we fetch it.
    //
    // So, we always say yes here, which allows us to generate an ERR_CACHE_MISS
    // if the request cannot be serviced from cache.  We force the 'DontLoad'
    // cache policy at this point to ensure that we never hit the network for
    // this request.
    //
    ASSERT(request.httpMethod() == "POST");
    request.setCachePolicy(ReturnCacheDataDontLoad);
    return true;
}

} // namespace WebCore
