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

#ifndef WebForm_h
#define WebForm_h

#include "WebCommon.h"

#if WEBKIT_IMPLEMENTATION
namespace WebCore { class HTMLFormElement; }
namespace WTF { template <typename T> class PassRefPtr; }
#endif

namespace WebKit {
    class WebFormPrivate;

    // A container for passing around a reference to a form element.  Provides
    // some information about the form.
    class WebForm {
    public:
        ~WebForm() { reset(); }

        WebForm() : m_private(0) { }
        WebForm(const WebForm& f) : m_private(0) { assign(f); }
        WebForm& operator=(const WebForm& f) { assign(f); return *this; }

        WEBKIT_API void reset();
        WEBKIT_API void assign(const WebForm&);

        bool isNull() const { return m_private == 0; }

        // Returns true if the form does not have "autocomplete=off" specified.
        WEBKIT_API bool isAutoCompleteEnabled() const;

#if WEBKIT_IMPLEMENTATION
        WebForm(const WTF::PassRefPtr<WebCore::HTMLFormElement>&);
        WebForm& operator=(const WTF::PassRefPtr<WebCore::HTMLFormElement>&);
        operator WTF::PassRefPtr<WebCore::HTMLFormElement>() const;
#endif

    private:
        void assign(WebFormPrivate*);
        WebFormPrivate* m_private;
    };

} // namespace WebKit

#endif
