// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_network_layer.h"

#include "base/logging.h"
#include "net/base/client_socket_factory.h"
#include "net/http/http_network_session.h"
#include "net/http/http_network_transaction.h"
#if defined(OS_WIN)
#include "net/http/http_transaction_winhttp.h"
#endif

namespace net {

//-----------------------------------------------------------------------------

#if defined(OS_WIN)
// static
bool HttpNetworkLayer::use_winhttp_ = false;
#endif

// static
HttpTransactionFactory* HttpNetworkLayer::CreateFactory(
    ProxyService* proxy_service) {
  DCHECK(proxy_service);
#if defined(OS_WIN)
  if (use_winhttp_)
    return new HttpTransactionWinHttp::Factory(proxy_service);
#endif

  return new HttpNetworkLayer(proxy_service);
}

#if defined(OS_WIN)
// static
void HttpNetworkLayer::UseWinHttp(bool value) {
  use_winhttp_ = value;
}
#endif

//-----------------------------------------------------------------------------

HttpNetworkLayer::HttpNetworkLayer(ProxyService* proxy_service)
    : proxy_service_(proxy_service), suspended_(false) {
  DCHECK(proxy_service_);
}

HttpNetworkLayer::~HttpNetworkLayer() {
}

HttpTransaction* HttpNetworkLayer::CreateTransaction() {
  if (suspended_)
    return NULL;

  if (!session_)
    session_ = new HttpNetworkSession(proxy_service_);

  return new HttpNetworkTransaction(
      session_, ClientSocketFactory::GetDefaultFactory());
}

HttpCache* HttpNetworkLayer::GetCache() {
  return NULL;
}

void HttpNetworkLayer::Suspend(bool suspend) {
  suspended_ = suspend;

  if (suspend && session_)
    session_->connection_pool()->CloseIdleSockets();
}

}  // namespace net

