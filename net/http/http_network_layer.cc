// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_network_layer.h"

#include "base/logging.h"
#include "net/base/client_socket_factory.h"
#include "net/http/http_network_session.h"
#include "net/http/http_network_transaction.h"

namespace net {

//-----------------------------------------------------------------------------

// static
HttpTransactionFactory* HttpNetworkLayer::CreateFactory(
    HostResolver* host_resolver,
    ProxyService* proxy_service) {
  DCHECK(proxy_service);

  return new HttpNetworkLayer(host_resolver, proxy_service);
}

// static
HttpTransactionFactory* HttpNetworkLayer::CreateFactory(
    HttpNetworkSession* session) {
  DCHECK(session);

  return new HttpNetworkLayer(session);
}

//-----------------------------------------------------------------------------

HttpNetworkLayer::HttpNetworkLayer(HostResolver* host_resolver,
                                   ProxyService* proxy_service)
    : host_resolver_(host_resolver),
      proxy_service_(proxy_service),
      session_(NULL),
      suspended_(false) {
  DCHECK(proxy_service_);
}

HttpNetworkLayer::HttpNetworkLayer(HttpNetworkSession* session)
    : proxy_service_(NULL), session_(session), suspended_(false) {
  DCHECK(session_.get());
}

HttpNetworkLayer::~HttpNetworkLayer() {
}

HttpTransaction* HttpNetworkLayer::CreateTransaction() {
  if (suspended_)
    return NULL;

  return new HttpNetworkTransaction(
      GetSession(), ClientSocketFactory::GetDefaultFactory());
}

HttpCache* HttpNetworkLayer::GetCache() {
  return NULL;
}

void HttpNetworkLayer::Suspend(bool suspend) {
  suspended_ = suspend;

  if (suspend && session_)
    session_->connection_pool()->CloseIdleSockets();
}

HttpNetworkSession* HttpNetworkLayer::GetSession() {
  if (!session_) {
    DCHECK(proxy_service_);
    session_ = new HttpNetworkSession(host_resolver_, proxy_service_,
                                      ClientSocketFactory::GetDefaultFactory());
  }
  return session_;
}

}  // namespace net
