// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_network_layer.h"

#include "base/notimplemented.h"
#include "net/base/client_socket_factory.h"
#include "net/http/http_network_session.h"
#include "net/http/http_network_transaction.h"
#include "net/proxy/proxy_resolver_fixed.h"
#if defined(OS_WIN)
#include "net/http/http_transaction_winhttp.h"
#include "net/proxy/proxy_resolver_winhttp.h"
#endif

namespace net {

//-----------------------------------------------------------------------------

#if defined(OS_WIN)
// static
bool HttpNetworkLayer::use_winhttp_ = true;
#endif

// static
HttpTransactionFactory* HttpNetworkLayer::CreateFactory(
    const ProxyInfo* pi) {
#if defined(OS_WIN)
  if (use_winhttp_)
    return new HttpTransactionWinHttp::Factory(pi);
#endif

  return new HttpNetworkLayer(pi);
}

#if defined(OS_WIN)
// static
void HttpNetworkLayer::UseWinHttp(bool value) {
  use_winhttp_ = value;
}
#endif

//-----------------------------------------------------------------------------

HttpNetworkLayer::HttpNetworkLayer(const ProxyInfo* pi)
    : suspended_(false) {
  ProxyResolver* proxy_resolver;
  if (pi) {
    proxy_resolver = new ProxyResolverFixed(*pi);
  } else {
#if defined(OS_WIN)
    proxy_resolver = new ProxyResolverWinHttp();
#else
    NOTIMPLEMENTED();
    proxy_resolver = NULL;
#endif
  }
  session_ = new HttpNetworkSession(proxy_resolver);
}

HttpNetworkLayer::~HttpNetworkLayer() {
}

HttpTransaction* HttpNetworkLayer::CreateTransaction() {
  if (suspended_)
    return NULL;

  return new HttpNetworkTransaction(
      session_, ClientSocketFactory::GetDefaultFactory());
}

HttpCache* HttpNetworkLayer::GetCache() {
  return NULL;
}

AuthCache* HttpNetworkLayer::GetAuthCache() {
  return session_->auth_cache();
}

void HttpNetworkLayer::Suspend(bool suspend) {
  suspended_ = suspend;

  if (suspend)
    session_->connection_pool()->CloseIdleSockets();
}

}  // namespace net

