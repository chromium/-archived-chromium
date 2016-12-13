// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_PROXY_CONFIG_SERVICE_FIXED_H_
#define NET_PROXY_PROXY_CONFIG_SERVICE_FIXED_H_

#include "net/proxy/proxy_config_service.h"

namespace net {

// Implementation of ProxyConfigService that returns a fixed result.
class ProxyConfigServiceFixed : public ProxyConfigService {
 public:
  explicit ProxyConfigServiceFixed(const ProxyConfig& pc) : pc_(pc) {}

  // ProxyConfigService methods:
  virtual int GetProxyConfig(ProxyConfig* config) {
    *config = pc_;
    return OK;
  }

 private:
  ProxyConfig pc_;
};

}  // namespace net

#endif  // NET_PROXY_PROXY_CONFIG_SERVICE_FIXED_H_
