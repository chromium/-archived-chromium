// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef JSXPATHNSRESOLVER_H__
#define JSXPATHNSRESOLVER_H__

#if ENABLE(XPATH)

#include <v8.h>
#include <wtf/RefCounted.h>
#include "XPathNSResolver.h"

namespace WebCore {

    class String;

    class JSXPathNSResolver : public XPathNSResolver {
    public:

        JSXPathNSResolver(v8::Handle<v8::Object> resolver);
        virtual ~JSXPathNSResolver();

        virtual String lookupNamespaceURI(const String& prefix);

    private:
        v8::Handle<v8::Object> m_resolver;  // Handle to resolver object.
    };
}

#endif  // ENABLE(XPATH)

#endif  // JSXPATHNSRESOLVER_H__
