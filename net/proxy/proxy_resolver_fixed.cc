// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/proxy_resolver_fixed.h"

#include "net/base/net_errors.h"

namespace net {

int ProxyResolverFixed::GetProxyConfig(ProxyConfig* config) {
  config->proxy_server = pi_.proxy_server();
  return OK;
}

int ProxyResolverFixed::GetProxyForURL(const GURL& query_url,
                                       const GURL& pac_url,
                                       ProxyInfo* results) {
  NOTREACHED() << "Should not be asked to do proxy auto config";
  return ERR_FAILED;
}

}  // namespace net

