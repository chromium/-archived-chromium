// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/proxy_config.h"

namespace net {

// static
ProxyConfig::ID ProxyConfig::last_id_ = ProxyConfig::INVALID_ID;

ProxyConfig::ProxyConfig()
    : auto_detect(false),
      proxy_bypass_local_names(false),
      id_(++last_id_) {
}

bool ProxyConfig::Equals(const ProxyConfig& other) const {
  // The two configs can have different IDs.  We are just interested in if they
  // have the same settings.
  return auto_detect == other.auto_detect &&
         pac_url == other.pac_url &&
         proxy_rules == other.proxy_rules &&
         proxy_bypass == other.proxy_bypass &&
         proxy_bypass_local_names == other.proxy_bypass_local_names;
}

}  // namespace net
