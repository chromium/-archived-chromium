// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_PROXY_CONFIG_SERVICE_H_
#define NET_PROXY_PROXY_CONFIG_SERVICE_H_

namespace net {

class ProxyConfig;

// Synchronously fetch the system's proxy configuration settings. Called on
// the IO Thread.
class ProxyConfigService {
 public:
  virtual ~ProxyConfigService() {}

  // Get the proxy configuration.  Returns OK if successful or an error code if
  // otherwise.  |config| should be in its initial state when this method is
  // called.
  virtual int GetProxyConfig(ProxyConfig* config) = 0;
};

}  // namespace net

#endif  // NET_PROXY_PROXY_CONFIG_SERVICE_H_
