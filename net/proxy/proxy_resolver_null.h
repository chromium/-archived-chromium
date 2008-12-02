// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_PROXY_RESOLVER_NULL_H_
#define NET_PROXY_PROXY_RESOLVER_NULL_H_

#include "net/proxy/proxy_service.h"

namespace net {

// Implementation of ProxyResolver that always fails.
class ProxyResolverNull : public ProxyResolver {
 public:
  virtual int GetProxyConfig(ProxyConfig* config) {
    return ERR_NOT_IMPLEMENTED;
  }
  virtual int GetProxyForURL(const GURL& query_url,
                             const GURL& pac_url,
                             ProxyInfo* results) {
    return ERR_NOT_IMPLEMENTED;
  }
};

}  // namespace net

#endif  // NET_PROXY_PROXY_RESOLVER_NULL_H_

