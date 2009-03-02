// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_PROXY_CONFIG_SERVICE_WIN_H_
#define NET_PROXY_PROXY_CONFIG_SERVICE_WIN_H_

#include "net/proxy/proxy_config_service.h"

namespace net {

// Implementation of ProxyConfigService that retrieves the system proxy
// settings.
class ProxyConfigServiceWin : public ProxyConfigService {
 public:
  // ProxyConfigService methods.
  virtual int GetProxyConfig(ProxyConfig* config);
};

}  // namespace net

#endif  // NET_PROXY_PROXY_CONFIG_SERVICE_WIN_H_

