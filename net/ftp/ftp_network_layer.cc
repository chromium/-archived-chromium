// Copyright (c) 2008 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "net/ftp/ftp_network_layer.h"

#include "net/base/client_socket_factory.h"
#include "net/ftp/ftp_network_session.h"
#include "net/ftp/ftp_network_transaction.h"

namespace net {

FtpNetworkLayer::FtpNetworkLayer()
    : suspended_(false) {
  session_ = new FtpNetworkSession();
}

FtpNetworkLayer::~FtpNetworkLayer() {
}

FtpTransaction* FtpNetworkLayer::CreateTransaction() {
  if (suspended_)
    return NULL;

  return new FtpNetworkTransaction(
      session_, ClientSocketFactory::GetDefaultFactory());
}

AuthCache* FtpNetworkLayer::GetAuthCache() {
  return session_->auth_cache();
}

void FtpNetworkLayer::Suspend(bool suspend) {
  suspended_ = suspend;

  /* TODO(darin): We'll need this code once we have a connection manager.
  if (suspend)
    session_->connection_manager()->CloseIdleSockets();
  */
}

}  // namespace net
