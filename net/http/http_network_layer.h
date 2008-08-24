// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_NETWORK_LAYER_H_
#define NET_HTTP_HTTP_NETWORK_LAYER_H_

#include "base/ref_counted.h"
#include "net/http/http_transaction_factory.h"

namespace net {

class HttpNetworkSession;
class ProxyInfo;

class HttpNetworkLayer : public HttpTransactionFactory {
 public:
  explicit HttpNetworkLayer(const ProxyInfo* pi);
  ~HttpNetworkLayer();

  // This function hides the details of how a network layer gets instantiated
  // and allows other implementations to be substituted.
  static HttpTransactionFactory* CreateFactory(const ProxyInfo* pi);

#if defined(OS_WIN)
  // If value is true, then WinHTTP will be used.
  static void UseWinHttp(bool value);
#endif

  // HttpTransactionFactory methods:
  virtual HttpTransaction* CreateTransaction();
  virtual HttpCache* GetCache();
  virtual AuthCache* GetAuthCache();
  virtual void Suspend(bool suspend);

 private:
#if defined(OS_WIN)
  static bool use_winhttp_;
#endif

  scoped_refptr<HttpNetworkSession> session_;
  bool suspended_;
};

}  // namespace net

#endif  // NET_HTTP_HTTP_NETWORK_LAYER_H_

