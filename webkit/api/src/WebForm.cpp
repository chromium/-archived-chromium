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
#include "WebForm.h"

#include "HTMLFormElement.h"
#include <wtf/PassRefPtr.h>

using namespace WebCore;

namespace WebKit {

class WebFormPrivate : public HTMLFormElement {
};

void WebForm::reset()
{
    assign(0);
}

void WebForm::assign(const WebForm& other)
{
    WebFormPrivate* p = const_cast<WebFormPrivate*>(other.m_private);
    p->ref();
    assign(p);
}

bool WebForm::isAutoCompleteEnabled() const
{
    ASSERT(!isNull());
    return m_private->autoComplete();
}

WebForm::WebForm(const WTF::PassRefPtr<WebCore::HTMLFormElement>& element)
    : m_private(static_cast<WebFormPrivate*>(element.releaseRef()))
{
}

WebForm& WebForm::operator=(const WTF::PassRefPtr<WebCore::HTMLFormElement>& element)
{
    assign(static_cast<WebFormPrivate*>(element.releaseRef()));
    return *this;
}

WebForm::operator WTF::PassRefPtr<WebCore::HTMLFormElement>() const
{
    return PassRefPtr<HTMLFormElement>(const_cast<WebFormPrivate*>(m_private));
}

void WebForm::assign(WebFormPrivate* p)
{
    // p is already ref'd for us by the caller
    if (m_private)
        m_private->deref();
    m_private = p;
}

} // namespace WebKit
