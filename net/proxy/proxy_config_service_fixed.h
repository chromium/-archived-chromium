// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_PROXY_CONFIG_SERVICE_FIXED_H_
#define NET_PROXY_PROXY_CONFIG_SERVICE_FIXED_H_

#include "net/proxy/proxy_config_service.h"

namespace net {

// Implementation of ProxyConfigService that returns a fixed result.
class ProxyConfigServiceFixed : public ProxyConfigService {
 public:
  explicit ProxyConfigServiceFixed(const ProxyInfo& pi) { pi_.Use(pi); }

  // ProxyConfigService methods:
  virtual int GetProxyConfig(ProxyConfig* config) {
    config->proxy_rules.type = ProxyConfig::ProxyRules::TYPE_SINGLE_PROXY;
    config->proxy_rules.single_proxy = pi_.proxy_server();
    return OK;
  }

 private:
  ProxyInfo pi_;
};

}  // namespace net

#endif  // NET_PROXY_PROXY_CONFIG_SERVICE_FIXED_H_
