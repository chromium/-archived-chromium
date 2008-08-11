// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef NET_HTTP_HTTP_NETWORK_SESSION_H_
#define NET_HTTP_HTTP_NETWORK_SESSION_H_

#include "base/ref_counted.h"
#include "net/base/auth_cache.h"
#include "net/base/client_socket_pool.h"
#include "net/proxy/proxy_service.h"

namespace net {

// This class holds session objects used by HttpNetworkTransaction objects.
class HttpNetworkSession : public base::RefCounted<HttpNetworkSession> {
 public:
  // Allow up to 6 connections per host.
  enum {
    MAX_SOCKETS_PER_GROUP = 6
  };
  
  HttpNetworkSession(ProxyResolver* proxy_resolver)
      : connection_pool_(new ClientSocketPool(MAX_SOCKETS_PER_GROUP)),
        proxy_resolver_(proxy_resolver),
        proxy_service_(proxy_resolver) {
  }

  AuthCache* auth_cache() { return &auth_cache_; }
  ClientSocketPool* connection_pool() { return connection_pool_; }
  ProxyService* proxy_service() { return &proxy_service_; }

 private:
  AuthCache auth_cache_;
  scoped_refptr<ClientSocketPool> connection_pool_;
  scoped_ptr<ProxyResolver> proxy_resolver_;
  ProxyService proxy_service_;
};

}  // namespace net

#endif  // NET_HTTP_HTTP_NETWORK_SESSION_H_
