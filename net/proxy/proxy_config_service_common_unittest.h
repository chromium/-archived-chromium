// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_PROXY_CONFIG_SERVICE_COMMON_UNITTEST_H_
#define NET_PROXY_PROXY_CONFIG_SERVICE_COMMON_UNITTEST_H_

#include <string>
#include <vector>

#include "net/proxy/proxy_config.h"

// A few small helper functions common to the win and linux unittests.

namespace net {

ProxyConfig::ProxyRules MakeProxyRules(
    ProxyConfig::ProxyRules::Type type,
    const char* single_proxy,
    const char* proxy_for_http,
    const char* proxy_for_https,
    const char* proxy_for_ftp);

ProxyConfig::ProxyRules MakeSingleProxyRules(const char* single_proxy);

ProxyConfig::ProxyRules MakeProxyPerSchemeRules(
    const char* proxy_http,
    const char* proxy_https,
    const char* proxy_ftp);

typedef std::vector<std::string> BypassList;

// Joins the proxy bypass list using "\n" to make it into a single string.
std::string FlattenProxyBypass(const BypassList& proxy_bypass);

}  // namespace net

#endif  // NET_PROXY_PROXY_CONFIG_SERVICE_COMMON_UNITTEST_H_
