// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/proxy_config.h"

#include "base/string_tokenizer.h"
#include "base/string_util.h"

namespace net {

ProxyConfig::ProxyConfig()
    : auto_detect(false),
      proxy_bypass_local_names(false),
      id_(INVALID_ID) {
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

bool ProxyConfig::MayRequirePACResolver() const {
  return auto_detect || !pac_url.is_empty();
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

      // If the proxy settings has only socks and others blank,
      // make that the default for all the proxies
      // This gets hit only on windows when using IE settings.
      if (url_scheme == "socks") {
        std::string proxy_server_string = "socks://";
        proxy_server_string.append(proxy_server_for_scheme.token());
        single_proxy = ProxyServer::FromURI(proxy_server_string);
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

namespace {

// Returns true if the given string represents an IP address.
bool IsIPAddress(const std::string& domain) {
  // From GURL::HostIsIPAddress()
  url_canon::RawCanonOutputT<char, 128> ignored_output;
  url_canon::CanonHostInfo host_info;
  url_parse::Component domain_comp(0, domain.size());
  url_canon::CanonicalizeIPAddress(domain.c_str(), domain_comp,
                                   &ignored_output, &host_info);
  return host_info.IsIPAddress();
}

}  // namespace

void ProxyConfig::ParseNoProxyList(const std::string& no_proxy) {
  proxy_bypass.clear();
  if (no_proxy.empty())
    return;
  // Traditional semantics:
  // A single "*" is specifically allowed and unproxies anything.
  // "*" wildcards other than a single "*" entry are not universally
  // supported. We will support them, as we get * wildcards for free
  // (see MatchPattern() called from ProxyService::ShouldBypassProxyForURL()).
  // no_proxy is a comma-separated list of <trailing_domain>[:<port>].
  // If no port is specified then any port matches.
  // The historical definition has trailing_domain match using a simple
  // string "endswith" test, so that the match need not correspond to a
  // "." boundary. For example: "google.com" matches "igoogle.com" too.
  // Seems like that could be confusing, but we'll obey tradition.
  // IP CIDR patterns are supposed to be supported too. We intend
  // to do this in proxy_service.cc, but it's currently a TODO.
  // See: http://crbug.com/9835.
  StringTokenizer no_proxy_list(no_proxy, ",");
  while (no_proxy_list.GetNext()) {
    std::string bypass_entry = no_proxy_list.token();
    TrimWhitespaceASCII(bypass_entry, TRIM_ALL, &bypass_entry);
    if (bypass_entry.empty())
      continue;
    if (bypass_entry.at(0) != '*') {
      // Insert a wildcard * to obtain an endsWith match, unless the
      // entry looks like it might be an IP or CIDR.
      // First look for either a :<port> or CIDR mask length suffix.
      std::string::const_iterator begin = bypass_entry.begin();
      std::string::const_iterator scan = bypass_entry.end() - 1;
      while (scan > begin && IsAsciiDigit(*scan))
        --scan;
      std::string potential_ip;
      if (*scan == '/' || *scan == ':')
        potential_ip = std::string(begin, scan - 1);
      else
        potential_ip = bypass_entry;
      if (!IsIPAddress(potential_ip)) {
        // Do insert a wildcard.
        bypass_entry.insert(0, "*");
      }
      // TODO(sdoyon): When CIDR matching is implemented in
      // proxy_service.cc, consider making proxy_bypass more
      // sophisticated to avoid parsing out the string on every
      // request.
    }
    proxy_bypass.push_back(bypass_entry);
  }
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

}  // namespace

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

  out << "  id: " << config.id() << "\n"
      << "}";
  return out;
}
