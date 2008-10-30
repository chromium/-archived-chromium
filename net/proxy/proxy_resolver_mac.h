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
  // ProxyResolver methods:
  virtual int GetProxyConfig(ProxyConfig* config);
  virtual int GetProxyForURL(const std::string& query_url,
                             const std::string& pac_url,
                             ProxyInfo* results);
};

}  // namespace net

#endif  // NET_PROXY_PROXY_RESOLVER_MAC_H_
