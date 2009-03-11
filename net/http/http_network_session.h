// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_NETWORK_SESSION_H_
#define NET_HTTP_HTTP_NETWORK_SESSION_H_

#include "base/ref_counted.h"
#include "net/base/client_socket_pool.h"
#include "net/base/ssl_config_service.h"
#include "net/http/http_auth_cache.h"

namespace net {

class ProxyService;

// This class holds session objects used by HttpNetworkTransaction objects.
class HttpNetworkSession : public base::RefCounted<HttpNetworkSession> {
 public:
  // Allow up to 6 connections per host.
  enum {
    MAX_SOCKETS_PER_GROUP = 6
  };

  explicit HttpNetworkSession(ProxyService* proxy_service)
      : connection_pool_(new ClientSocketPool(MAX_SOCKETS_PER_GROUP)),
        proxy_service_(proxy_service) {
    DCHECK(proxy_service);
  }

  HttpAuthCache* auth_cache() { return &auth_cache_; }
  ClientSocketPool* connection_pool() { return connection_pool_; }
  ProxyService* proxy_service() { return proxy_service_; }
#if defined(OS_WIN)
  SSLConfigService* ssl_config_service() { return &ssl_config_service_; }
#endif

 private:
  HttpAuthCache auth_cache_;
  scoped_refptr<ClientSocketPool> connection_pool_;
  ProxyService* proxy_service_;
#if defined(OS_WIN)
  // TODO(port): Port the SSLConfigService class to Linux and Mac OS X.
  SSLConfigService ssl_config_service_;
#endif
};

}  // namespace net

#endif  // NET_HTTP_HTTP_NETWORK_SESSION_H_
