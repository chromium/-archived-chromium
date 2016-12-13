// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_PROXY_CONFIG_SERVICE_WIN_H_
#define NET_PROXY_PROXY_CONFIG_SERVICE_WIN_H_

#include <windows.h>
#include <winhttp.h>

#include "net/proxy/proxy_config_service.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

namespace net {

// Implementation of ProxyConfigService that retrieves the system proxy
// settings.
class ProxyConfigServiceWin : public ProxyConfigService {
 public:
  // ProxyConfigService methods:
  virtual int GetProxyConfig(ProxyConfig* config);

 private:
  FRIEND_TEST(ProxyConfigServiceWinTest, SetFromIEConfig);

  // Set |config| using the proxy configuration values of |ie_config|.
  static void SetFromIEConfig(
      ProxyConfig* config,
      const WINHTTP_CURRENT_USER_IE_PROXY_CONFIG& ie_config);
};

}  // namespace net

#endif  // NET_PROXY_PROXY_CONFIG_SERVICE_WIN_H_
