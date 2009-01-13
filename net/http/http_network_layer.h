// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_NETWORK_LAYER_H_
#define NET_HTTP_HTTP_NETWORK_LAYER_H_

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "net/http/http_transaction_factory.h"

namespace net {

class HttpNetworkSession;
class ProxyInfo;
class ProxyService;

class HttpNetworkLayer : public HttpTransactionFactory {
 public:
  // |proxy_service| must remain valid for the lifetime of HttpNetworkLayer.
  explicit HttpNetworkLayer(ProxyService* proxy_service);
  ~HttpNetworkLayer();

  // This function hides the details of how a network layer gets instantiated
  // and allows other implementations to be substituted.
  static HttpTransactionFactory* CreateFactory(ProxyService* proxy_service);

  // HttpTransactionFactory methods:
  virtual HttpTransaction* CreateTransaction();
  virtual HttpCache* GetCache();
  virtual void Suspend(bool suspend);

 private:
  // The proxy service being used for the session.
  ProxyService* proxy_service_;

  scoped_refptr<HttpNetworkSession> session_;
  bool suspended_;
};

}  // namespace net

#endif  // NET_HTTP_HTTP_NETWORK_LAYER_H_

