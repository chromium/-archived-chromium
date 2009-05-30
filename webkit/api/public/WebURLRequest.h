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

#ifndef WebURLRequest_h
#define WebURLRequest_h

#error "This header file is still a work in progress; do not include!"

#include "WebCommon.h"
#include "WebHTTPBody.h"

namespace WebKit {
    class WebCString;
    class WebHTTPBody;
    class WebString;
    class WebURL;
    class WebURLRequestPrivate;

    class WebURLRequest {
    public:
        enum CachePolicy {
            UseProtocolCachePolicy,  // normal load
            ReloadIgnoringCacheData, // reload
            ReturnCacheDataElseLoad, // back/forward or encoding change - allow stale data
            ReturnCacheDataDontLoad, // results of a post - allow stale data and only use cache
        };

        enum TargetType {
            TargetIsMainFrame,
            TargetIsSubFrame,
            TargetIsSubResource,
            TargetIsObject,
            TargetIsMedia
        };

        ~WebURLRequest() { reset(); }

        WebURLRequest() : m_private(0) { }
        WebURLRequest(const WebURLRequest& r) : m_private(0) { assign(r); }
        WebURLRequest& operator=(const WebURLRequest& r) { assign(r); return *this; }

        WEBKIT_API void initialize();
        WEBKIT_API void reset();
        WEBKIT_API void assign(const WebURLRequest&);

        bool isNull() const { return m_private == 0; }

        WEBKIT_API WebURL url() const = 0;
        WEBKIT_API void setURL(const WebURL&) = 0;

        // Used to implement third-party cookie blocking.
        WEBKIT_API WebURL firstPartyForCookies() const = 0;
        WEBKIT_API void setFirstPartyForCookies(const WebURL&) = 0;

        WEBKIT_API CachePolicy cachePolicy() const = 0;
        WEBKIT_API void setCachePolicy(CachePolicy) = 0;

        WEBKIT_API WebString httpMethod() const = 0;
        WEBKIT_API void setHTTPMethod(const WebString&) = 0;

        WEBKIT_API WebString httpHeaderField(const WebString& name) const = 0;
        WEBKIT_API void setHTTPHeaderField(const WebString& name, const WebString& value) = 0;
        WEBKIT_API void addHTTPHeaderField(const WebString& name, const WebString& value) = 0;
        WEBKIT_API void clearHTTPHeaderField(const WebString& name) = 0;

        WEBKIT_API bool hasHTTPBody() const = 0;
        WEBKIT_API void httpBody(WebHTTPBody&) const = 0;
        WEBKIT_API void setHTTPBody(const WebHTTPBody&) = 0;
        WEBKIT_API void appendToHTTPBody(const WebHTTPBody::Element&) = 0;
        
        // Controls whether upload progress events are generated when a request
        // has a body.
        WEBKIT_API bool reportUploadProgress() const = 0;
        WEBKIT_API void setReportUploadProgress(bool) = 0;

        WEBKIT_API TargetType targetType() const = 0;
        WEBKIT_API void setTargetType(const TargetType&) = 0;

        // A consumer controlled value intended to be used to identify the
        // requestor.
        WEBKIT_API int requestorID() const = 0;
        WEBKIT_API void setRequestorID(int) = 0;

        // A consumer controlled value intended to be used to identify the
        // process of the requestor.
        WEBKIT_API int requestorProcessID() const = 0;
        WEBKIT_API void setRequestorProcessID(int) = 0;

        // A consumer controlled value intended to be used to record opaque
        // security info related to this request.
        // FIXME: This really doesn't belong here!
        WEBKIT_API WebCString securityInfo() const = 0;
        WEBKIT_API void setSecurityInfo(const WebCString&) = 0;

    private:
        WebURLRequestPrivate* m_private;
    };

} // namespace WebKit

#endif
