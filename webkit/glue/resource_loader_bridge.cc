// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "config.h"

#include "webkit/glue/resource_loader_bridge.h"
#include "webkit/glue/webappcachecontext.h"

#include "net/http/http_response_headers.h"

namespace webkit_glue {

ResourceLoaderBridge::ResponseInfo::ResponseInfo() {
  content_length = -1;
  app_cache_id = WebAppCacheContext::kNoAppCacheId;
}

ResourceLoaderBridge::ResponseInfo::~ResponseInfo() {
}

ResourceLoaderBridge::SyncLoadResponse::SyncLoadResponse() {
}

ResourceLoaderBridge::SyncLoadResponse::~SyncLoadResponse() {
}

ResourceLoaderBridge::ResourceLoaderBridge() {
}

ResourceLoaderBridge::~ResourceLoaderBridge() {
}

}  // namespace webkit_glue
