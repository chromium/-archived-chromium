// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/proxy_config.h"

#include "base/string_tokenizer.h"
#include "base/string_util.h"

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

void ProxyConfig::ProxyRules::ParseFromString(const std::string& proxy_rules) {
  // Reset.
  type = TYPE_NO_RULES;
  single_proxy = ProxyServer();
  proxy_for_http = ProxyServer();
  proxy_for_https = ProxyServer();
  proxy_for_ftp = ProxyServer();

  StringTokenizer proxy_server_list(proxy_rules, ";");
  while (proxy_server_list.GetNext()) {
    StringTokenizer proxy_server_for_scheme(
        proxy_server_list.token_begin(), proxy_server_list.token_end(), "=");

    while (proxy_server_for_scheme.GetNext()) {
      std::string url_scheme = proxy_server_for_scheme.token();

      // If we fail to get the proxy server here, it means that
      // this is a regular proxy server configuration, i.e. proxies
      // are not configured per protocol.
      if (!proxy_server_for_scheme.GetNext()) {
        if (type == TYPE_PROXY_PER_SCHEME)
          continue;  // Unexpected.
        single_proxy = ProxyServer::FromURI(url_scheme);
        type = TYPE_SINGLE_PROXY;
        return;
      }

      // Trim whitespace off the url scheme.
      TrimWhitespaceASCII(url_scheme, TRIM_ALL, &url_scheme);

      // Add it to the per-scheme mappings (if supported scheme).
      type = TYPE_PROXY_PER_SCHEME;
      if (const ProxyServer* entry = MapSchemeToProxy(url_scheme))
        *const_cast<ProxyServer*>(entry) =
            ProxyServer::FromURI(proxy_server_for_scheme.token());
    }
  }
}

const ProxyServer* ProxyConfig::ProxyRules::MapSchemeToProxy(
    const std::string& scheme) const {
  DCHECK(type == TYPE_PROXY_PER_SCHEME);
  if (scheme == "http")
    return &proxy_for_http;
  if (scheme == "https")
    return &proxy_for_https;
  if (scheme == "ftp")
    return &proxy_for_ftp;
  return NULL;  // No mapping for this scheme.
}

}  // namespace net

namespace {

// Helper to stringize a ProxyServer.
std::ostream& operator<<(std::ostream& out,
                         const net::ProxyServer& proxy_server) {
  if (proxy_server.is_valid())
    out << proxy_server.ToURI();
  return out;
}

// Helper to stringize a ProxyRules.
std::ostream& operator<<(std::ostream& out,
                         const net::ProxyConfig::ProxyRules& rules) {
  // Stringize the type enum.
  std::string type;
  switch (rules.type) {
    case net::ProxyConfig::ProxyRules::TYPE_NO_RULES:
      type = "TYPE_NO_RULES";
      break;
    case net::ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME:
      type = "TYPE_PROXY_PER_SCHEME";
      break;
    case net::ProxyConfig::ProxyRules::TYPE_SINGLE_PROXY:
      type = "TYPE_SINGLE_PROXY";
      break;
    default:
      type = IntToString(rules.type);
      break;
  }
  return out << "  {\n"
             << "    type: " << type << "\n"
             << "    single_proxy: " << rules.single_proxy << "\n"
             << "    proxy_for_http: " << rules.proxy_for_http << "\n"
             << "    proxy_for_https: " << rules.proxy_for_https << "\n"
             << "    proxy_for_ftp: " << rules.proxy_for_ftp << "\n"
             << "  }";
}

}  // namespace

std::ostream& operator<<(std::ostream& out, const net::ProxyConfig& config) {
  out << "{\n"
      << "  auto_detect: " << config.auto_detect << "\n"
      << "  pac_url: " << config.pac_url << "\n"
      << "  proxy_rules:\n" << config.proxy_rules << "\n"
      << "  proxy_bypass_local_names: " << config.proxy_bypass_local_names
      << "\n"
      << "  proxy_bypass_list:\n";

  // Print out the proxy bypass list.
  if (!config.proxy_bypass.empty()) {
    out << "  {\n";
    std::vector<std::string>::const_iterator it;
    for (it = config.proxy_bypass.begin();
         it != config.proxy_bypass.end(); ++it) {
      out << "    " << *it << "\n";
    }
    out << "  }\n";
  }

  out << "}";
  return out;
}
