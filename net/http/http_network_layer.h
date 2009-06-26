// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_NETWORK_LAYER_H_
#define NET_HTTP_HTTP_NETWORK_LAYER_H_

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "net/http/http_transaction_factory.h"

namespace net {

class ClientSocketFactory;
class HostResolver;
class HttpNetworkSession;
class ProxyInfo;
class ProxyService;

class HttpNetworkLayer : public HttpTransactionFactory {
 public:
  // |socket_factory|, |proxy_service| and |host_resolver| must remain valid
  // for the lifetime of HttpNetworkLayer.
  HttpNetworkLayer(ClientSocketFactory* socket_factory,
                   HostResolver* host_resolver, ProxyService* proxy_service);
  // Construct a HttpNetworkLayer with an existing HttpNetworkSession which
  // contains a valid ProxyService.
  explicit HttpNetworkLayer(HttpNetworkSession* session);
  ~HttpNetworkLayer();

  // This function hides the details of how a network layer gets instantiated
  // and allows other implementations to be substituted.
  static HttpTransactionFactory* CreateFactory(HostResolver* host_resolver,
                                               ProxyService* proxy_service);
  // Create a transaction factory that instantiate a network layer over an
  // existing network session. Network session contains some valuable
  // information (e.g. authentication data) that we want to share across
  // multiple network layers. This method exposes the implementation details
  // of a network layer, use this method with an existing network layer only
  // when network session is shared.
  static HttpTransactionFactory* CreateFactory(HttpNetworkSession* session);

  // HttpTransactionFactory methods:
  virtual HttpTransaction* CreateTransaction();
  virtual HttpCache* GetCache();
  virtual void Suspend(bool suspend);

  HttpNetworkSession* GetSession();

 private:
  // The factory we will use to create network sockets.
  ClientSocketFactory* socket_factory_;

  // The host resolver being used for the session.
  scoped_refptr<HostResolver> host_resolver_;

  // The proxy service being used for the session.
  ProxyService* proxy_service_;

  scoped_refptr<HttpNetworkSession> session_;
  bool suspended_;
};

}  // namespace net

#endif  // NET_HTTP_HTTP_NETWORK_LAYER_H_
