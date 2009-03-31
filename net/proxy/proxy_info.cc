// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/proxy_info.h"

namespace net {

ProxyInfo::ProxyInfo()
    : config_id_(ProxyConfig::INVALID_ID),
      config_was_tried_(false) {
}

void ProxyInfo::Use(const ProxyInfo& other) {
  proxy_list_ = other.proxy_list_;
}

void ProxyInfo::UseDirect() {
  proxy_list_.Set(std::string());
}

void ProxyInfo::UseNamedProxy(const std::string& proxy_uri_list) {
  proxy_list_.Set(proxy_uri_list);
}

void ProxyInfo::UseProxyServer(const ProxyServer& proxy_server) {
  proxy_list_.SetSingleProxyServer(proxy_server);
}

std::string ProxyInfo::ToPacString() {
  return proxy_list_.ToPacString();
}

}  // namespace net
