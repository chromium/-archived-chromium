/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"

#pragma warning(push, 0)
#include "Document.h"
#include "KURL.h"
#include "PlatformString.h"
#include "CString.h"
#include "DeprecatedString.h"
#include "Vector.h"
#pragma warning(pop)

#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/glue_util.h"

namespace WebCore
{

void setCookies(Document* document, const KURL& url, const KURL& policyURL, const String& value)
{
    // We ignore the policyURL and compute it directly ourselves to ensure
    // consistency with the cookies() method below.
    KURL policyBaseURL(document->policyBaseURL().deprecatedString());
    WebCore::CString utf8value = value.utf8();
    webkit_glue::SetCookie(
        webkit_glue::KURLToGURL(url),
        webkit_glue::KURLToGURL(policyBaseURL),
        std::string(utf8value.data(), utf8value.length()));
}

String cookies(const Document* document, const KURL& url)
{
    KURL policyBaseURL(document->policyBaseURL().deprecatedString());
    std::string result = 
        webkit_glue::GetCookies(webkit_glue::KURLToGURL(url),
                                webkit_glue::KURLToGURL(policyBaseURL));
    return String(result.data(), result.size());
}

bool cookiesEnabled(const Document*)
{
    // FIXME: For now just assume cookies are always on.
    return true;
}

}
