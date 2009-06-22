// Copyright (c) 2008 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "net/ftp/ftp_network_layer.h"

#include "net/ftp/ftp_network_session.h"
#include "net/ftp/ftp_network_transaction.h"
#include "net/socket/client_socket_factory.h"

namespace net {

FtpNetworkLayer::FtpNetworkLayer(HostResolver* host_resolver)
    : session_(new FtpNetworkSession(host_resolver)),
      suspended_(false) {
}

FtpNetworkLayer::~FtpNetworkLayer() {
}

// static
FtpTransactionFactory* FtpNetworkLayer::CreateFactory(
    HostResolver* host_resolver) {
  return new FtpNetworkLayer(host_resolver);
}

FtpTransaction* FtpNetworkLayer::CreateTransaction() {
  if (suspended_)
    return NULL;

  return new FtpNetworkTransaction(
      session_, ClientSocketFactory::GetDefaultFactory());
}

void FtpNetworkLayer::Suspend(bool suspend) {
  suspended_ = suspend;

  /* TODO(darin): We'll need this code once we have a connection manager.
  if (suspend)
    session_->connection_manager()->CloseIdleSockets();
  */
}

}  // namespace net
