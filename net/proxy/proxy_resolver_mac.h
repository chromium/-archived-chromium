// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_PROXY_RESOLVER_MAC_H_
#define NET_PROXY_PROXY_RESOLVER_MAC_H_

#include "net/proxy/proxy_service.h"

namespace net {

// Implementation of ProxyResolver that uses the Mac CFProxySupport to implement
// proxies.
class ProxyResolverMac : public ProxyResolver {
 public:
  ProxyResolverMac() : ProxyResolver(true) {}

  // ProxyResolver methods:
  virtual int GetProxyForURL(const GURL& query_url,
                             const GURL& pac_url,
                             ProxyInfo* results);
};

class ProxyConfigServiceMac : public ProxyConfigService {
 public:
  // ProxyConfigService methods:
  virtual int GetProxyConfig(ProxyConfig* config);
};

}  // namespace net

#endif  // NET_PROXY_PROXY_RESOLVER_MAC_H_
