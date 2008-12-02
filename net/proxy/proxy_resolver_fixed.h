// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_PROXY_RESOLVER_FIXED_H_
#define NET_PROXY_PROXY_RESOLVER_FIXED_H_

#include "net/proxy/proxy_service.h"

namespace net {

// Implementation of ProxyResolver that returns a fixed result.
class ProxyResolverFixed : public ProxyResolver {
 public:
  ProxyResolverFixed(const ProxyInfo& pi) { pi_.Use(pi); }

  // ProxyResolver methods:
  virtual int GetProxyConfig(ProxyConfig* config);
  virtual int GetProxyForURL(const GURL& query_url,
                             const GURL& pac_url,
                             ProxyInfo* results);

 private:
  ProxyInfo pi_;
};

}  // namespace net

#endif  // NET_PROXY_PROXY_RESOLVER_FIXED_H_

