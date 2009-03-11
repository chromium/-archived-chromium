// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_PROXY_CONFIG_H_
#define NET_PROXY_PROXY_CONFIG_H_

#include <string>
#include <vector>

#include "googleurl/src/gurl.h"

namespace net {

// Proxy configuration used to by the ProxyService.
class ProxyConfig {
 public:
  typedef int ID;

  // Indicates an invalid proxy config.
  enum { INVALID_ID = 0 };

  ProxyConfig();
  // Default copy-constructor and assignment operator are OK!

  // Used to numerically identify this configuration.
  ID id() const { return id_; }

  // True if the proxy configuration should be auto-detected.
  bool auto_detect;

  // If non-empty, indicates the URL of the proxy auto-config file to use.
  GURL pac_url;

  // If non-empty, indicates the proxy server to use, given by:
  //
  //   proxy-uri = [<proxy-scheme>://]<proxy-host>[:"<proxy-port>]
  //
  // If the proxy to use depends on the scheme of the URL, can instead specify
  // a semicolon separated list of:
  //
  //   <url-scheme>"="<proxy-uri>
  //
  // For example:
  //   "http=foopy:80;ftp=foopy2"  -- use HTTP proxy "foopy:80" for http URLs,
  //                                  and HTTP proxy "foopy2:80" for ftp URLs.
  //   "foopy:80"                  -- use HTTP proxy "foopy:80" for all URLs.
  //   "socks4://foopy"            -- use SOCKS v4 proxy "foopy:1080" for all
  //                                  URLs.
  std::string proxy_rules;

  // Indicates a list of hosts that should bypass any proxy configuration.  For
  // these hosts, a direct connection should always be used.
  std::vector<std::string> proxy_bypass;

  // Indicates whether local names (no dots) bypass proxies.
  bool proxy_bypass_local_names;

  // Returns true if the given config is equivalent to this config.
  bool Equals(const ProxyConfig& other) const;

 private:
  static int last_id_;
  int id_;
};

}  // namespace net

#endif  // NET_PROXY_PROXY_CONFIG_H_
