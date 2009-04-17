// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/webappcachecontext.h"

namespace {

WebAppCacheContext::WebAppCacheFactoryProc factory_proc = NULL;

class NoopWebAppCacheContext : public WebAppCacheContext {
 public:
  virtual int GetContextID() { return kNoAppCacheContextId; }
  virtual int64 GetAppCacheID() { return kNoAppCacheId; }
  virtual void Initialize(ContextType context_type,
                          WebAppCacheContext* opt_parent) {}
  virtual void SelectAppCacheWithoutManifest(
                                     const GURL& document_url,
                                     int64 cache_document_was_loaded_from) {}
  virtual void SelectAppCacheWithManifest(
                                     const GURL& document_url,
                                     int64 cache_document_was_loaded_from,
                                     const GURL& manifest_url) {}

};

}  // namespace

const int WebAppCacheContext::kNoAppCacheContextId = 0;
const int64 WebAppCacheContext::kNoAppCacheId = 0;
const int64 WebAppCacheContext::kUnknownAppCacheId = -1;

WebAppCacheContext* WebAppCacheContext::Create() {
  if (factory_proc)
    return factory_proc();
  return new NoopWebAppCacheContext();
}

void WebAppCacheContext::SetFactory(WebAppCacheFactoryProc proc) {
  factory_proc = proc;
}
