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

#ifndef WebURLResponse_h
#define WebURLResponse_h

#error "This header file is still a work in progress; do not include!"

#include "WebCommon.h"

namespace WebKit {
    class WebCString;
    class WebString;
    class WebURL;
    class WebURLResponsePrivate;

    class WebURLResponse {
    public:
        ~WebURLResponse() { reset(); }

        WebURLResponse() : m_private(0) { }
        WebURLResponse(const WebURLResponse& r) : m_private(0) { assign(r); }
        WebURLResponse& operator=(const WebURLResponse& r) { assign(r); return *this; }

        WEBKIT_API void initialize();
        WEBKIT_API void reset();
        WEBKIT_API void assign(const WebURLResponse&);

        bool isNull() const { return m_private == 0; }

        WEBKIT_API WebURL url() const = 0;
        WEBKIT_API void setURL(const WebURL&) = 0;

        WEBKIT_API WebString mimeType() const = 0;
        WEBKIT_API void setMIMEType(const WebString&) = 0;

        WEBKIT_API long long expectedContentLength() const = 0;
        WEBKIT_API void setExpectedContentLength(long long) = 0;

        WEBKIT_API WebString textEncodingName() const = 0;
        WEBKIT_API void setTextEncodingName(const WebString&) = 0;

        WEBKIT_API WebString suggestedFileName() const = 0;
        WEBKIT_API void setSuggestedFileName(const WebString&) = 0;

        WEBKIT_API int httpStatusCode() const = 0;
        WEBKIT_API void setHTTPStatusCode(int) = 0;

        WEBKIT_API WebString httpStatusText() const = 0;
        WEBKIT_API void setHTTPStatusText(const WebString&) = 0;

        WEBKIT_API WebString httpHeaderField(const WebString& name) const = 0;
        WEBKIT_API void setHTTPHeaderField(const WebString& name, const WebString& value) = 0;
        WEBKIT_API void addHTTPHeaderField(const WebString& name, const WebString& value) = 0;
        WEBKIT_API void clearHTTPHeaderField(const WebString& name) = 0;

        WEBKIT_API double expirationDate() const = 0;
        WEBKIT_API void setExpirationDate(double) = 0;

        WEBKIT_API double lastModifiedDate() const = 0;
        WEBKIT_API void setLastModifiedDate(double) = 0;

        WEBKIT_API bool isContentFiltered() const = 0;
        WEBKIT_API void setIsContentFiltered(bool) = 0;

        // A consumer controlled value intended to be used to record opaque
        // security info related to this request.
        WEBKIT_API WebCString securityInfo() const = 0;
        WEBKIT_API void setSecurityInfo(const WebCString&) = 0;

    private:
        WebURLResponsePrivate* m_private;
    };

} // namespace WebKit

#endif
