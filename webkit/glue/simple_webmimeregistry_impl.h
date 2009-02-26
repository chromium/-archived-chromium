// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef WEBMIMEREGISTRY_IMPL_H_
#define WEBMIMEREGISTRY_IMPL_H_

#include "WebMimeRegistry.h"

namespace webkit_glue {

class SimpleWebMimeRegistryImpl : public WebKit::WebMimeRegistry {
 public:
  // WebMimeRegistry methods:
  virtual bool supportsImageMIMEType(const WebKit::WebString&);
  virtual bool supportsJavaScriptMIMEType(const WebKit::WebString&);
  virtual bool supportsNonImageMIMEType(const WebKit::WebString&);
  virtual WebKit::WebString mimeTypeForExtension(const WebKit::WebString&);
  virtual WebKit::WebString mimeTypeFromFile(const WebKit::WebString&);
  virtual WebKit::WebString preferredExtensionForMIMEType(
      const WebKit::WebString&);
};

}  // namespace webkit_glue

#endif  // WEBMIMEREGISTRY_IMPL_H_
