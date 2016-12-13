// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/proxy_config_service_common_unittest.h"

#include <string>
#include <vector>

#include "net/proxy/proxy_config.h"

namespace net {

ProxyConfig::ProxyRules MakeProxyRules(
    ProxyConfig::ProxyRules::Type type,
    const char* single_proxy,
    const char* proxy_for_http,
    const char* proxy_for_https,
    const char* proxy_for_ftp) {
  ProxyConfig::ProxyRules rules;
  rules.type = type;
  rules.single_proxy = ProxyServer::FromURI(single_proxy);
  rules.proxy_for_http = ProxyServer::FromURI(proxy_for_http);
  rules.proxy_for_https = ProxyServer::FromURI(proxy_for_https);
  rules.proxy_for_ftp = ProxyServer::FromURI(proxy_for_ftp);
  return rules;
}

ProxyConfig::ProxyRules MakeSingleProxyRules(const char* single_proxy) {
  return MakeProxyRules(ProxyConfig::ProxyRules::TYPE_SINGLE_PROXY,
                        single_proxy, "", "", "");
}

ProxyConfig::ProxyRules MakeProxyPerSchemeRules(
    const char* proxy_http,
    const char* proxy_https,
    const char* proxy_ftp) {
  return MakeProxyRules(ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME,
                        "", proxy_http, proxy_https, proxy_ftp);
}

std::string FlattenProxyBypass(const BypassList& proxy_bypass) {
  std::string flattened_proxy_bypass;
  for (BypassList::const_iterator it = proxy_bypass.begin();
       it != proxy_bypass.end(); ++it) {
    flattened_proxy_bypass += *it + "\n";
  }
  return flattened_proxy_bypass;
}

}  // namespace net
