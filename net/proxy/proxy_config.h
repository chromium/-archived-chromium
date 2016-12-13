// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_PROXY_CONFIG_H_
#define NET_PROXY_PROXY_CONFIG_H_

#include <ostream>
#include <string>
#include <vector>

#include "googleurl/src/gurl.h"
#include "net/proxy/proxy_server.h"

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
  void set_id(int id) { id_ = id; }
  bool is_valid() { return id_ != INVALID_ID; }

  // True if the proxy configuration should be auto-detected.
  bool auto_detect;

  // If non-empty, indicates the URL of the proxy auto-config file to use.
  GURL pac_url;

  struct ProxyRules {
    enum Type {
      TYPE_NO_RULES,
      TYPE_SINGLE_PROXY,
      TYPE_PROXY_PER_SCHEME,
    };

    // Note that the default of TYPE_NO_RULES results in direct connections
    // being made when using this ProxyConfig.
    ProxyRules() : type(TYPE_NO_RULES) {}

    bool empty() const {
      return type == TYPE_NO_RULES;
    }

    // Parses the rules from a string, indicating which proxies to use.
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
    void ParseFromString(const std::string& proxy_rules);

    // Returns one of {&proxy_for_http, &proxy_for_https, &proxy_for_ftp},
    // or NULL if it is a scheme that we don't have a mapping for. Should only
    // call this if the type is TYPE_PROXY_PER_SCHEME.
    const ProxyServer* MapSchemeToProxy(const std::string& scheme) const;

    bool operator==(const ProxyRules& other) const {
      return type == other.type &&
             single_proxy == other.single_proxy &&
             proxy_for_http == other.proxy_for_http &&
             proxy_for_https == other.proxy_for_https &&
             proxy_for_ftp == other.proxy_for_ftp;
    }

    Type type;

    // Set if |type| is TYPE_SINGLE_PROXY.
    ProxyServer single_proxy;

    // Set if |type| is TYPE_PROXY_PER_SCHEME.
    ProxyServer proxy_for_http;
    ProxyServer proxy_for_https;
    ProxyServer proxy_for_ftp;
  };

  ProxyRules proxy_rules;

  // Parses entries from a comma-separated list of hosts for which proxy
  // configurations should be bypassed. Clears proxy_bypass and sets it to the
  // resulting list.
  void ParseNoProxyList(const std::string& no_proxy);

  // Indicates a list of hosts that should bypass any proxy configuration.  For
  // these hosts, a direct connection should always be used.
  // The form <host>:<port> is also supported, meaning that only
  // connections on the specified port should be direct.
  std::vector<std::string> proxy_bypass;

  // Indicates whether local names (no dots) bypass proxies.
  bool proxy_bypass_local_names;

  // Returns true if the given config is equivalent to this config.
  bool Equals(const ProxyConfig& other) const;

  // Returns true if this config could possibly require the proxy service to
  // use a PAC resolver.
  bool MayRequirePACResolver() const;

 private:
  int id_;
};

}  // namespace net

// Dumps a human-readable string representation of the configuration to |out|;
// used when logging the configuration changes.
std::ostream& operator<<(std::ostream& out, const net::ProxyConfig& config);

// Dumps a human-readable string representation of the |rules| to |out|;
// used for logging and for better unittest failure output.
std::ostream& operator<<(std::ostream& out,
                         const net::ProxyConfig::ProxyRules& rules);

#endif  // NET_PROXY_PROXY_CONFIG_H_
