// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/proxy_server.h"

#include <algorithm>

#include "base/string_tokenizer.h"
#include "base/string_util.h"
#include "net/base/net_util.h"
#include "net/http/http_util.h"

namespace net {

namespace {

// Parse the proxy type from a PAC string, to a ProxyServer::Scheme.
// This mapping is case-insensitive. If no type could be matched
// returns SCHEME_INVALID.
ProxyServer::Scheme GetSchemeFromPacType(std::string::const_iterator begin,
                                         std::string::const_iterator end) {
  if (LowerCaseEqualsASCII(begin, end, "proxy"))
    return ProxyServer::SCHEME_HTTP;
  if (LowerCaseEqualsASCII(begin, end, "socks")) {
    // Default to v4 for compatibility. This is because the SOCKS4 vs SOCKS5
    // notation didn't originally exist, so if a client returns SOCKS they
    // really meant SOCKS4.
    return ProxyServer::SCHEME_SOCKS4;
  }
  if (LowerCaseEqualsASCII(begin, end, "socks4"))
    return ProxyServer::SCHEME_SOCKS4;
  if (LowerCaseEqualsASCII(begin, end, "socks5"))
    return ProxyServer::SCHEME_SOCKS5;
  if (LowerCaseEqualsASCII(begin, end, "direct"))
    return ProxyServer::SCHEME_DIRECT;

  return ProxyServer::SCHEME_INVALID;
}

// Parse the proxy scheme from a URL-like representation, to a
// ProxyServer::Scheme.  This corresponds with the values used in
// ProxyServer::ToURI(). If no type could be matched, returns SCHEME_INVALID.
ProxyServer::Scheme GetSchemeFromURI(std::string::const_iterator begin,
                                     std::string::const_iterator end) {
  if (LowerCaseEqualsASCII(begin, end, "http"))
    return ProxyServer::SCHEME_HTTP;
  if (LowerCaseEqualsASCII(begin, end, "socks4"))
    return ProxyServer::SCHEME_SOCKS4;
  if (LowerCaseEqualsASCII(begin, end, "socks"))
    return ProxyServer::SCHEME_SOCKS4;
  if (LowerCaseEqualsASCII(begin, end, "socks5"))
    return ProxyServer::SCHEME_SOCKS5;
  if (LowerCaseEqualsASCII(begin, end, "direct"))
    return ProxyServer::SCHEME_DIRECT;
  return ProxyServer::SCHEME_INVALID;
}

}  // namespace

std::string ProxyServer::HostNoBrackets() const {
  // Doesn't make sense to call this if the URI scheme doesn't
  // have concept of a host.
  DCHECK(is_valid() && !is_direct());

  // Remove brackets from an RFC 2732-style IPv6 literal address.
  const std::string::size_type len = host_.size();
  if (len != 0 && host_[0] == '[' && host_[len - 1] == ']')
    return host_.substr(1, len - 2);
  return host_;
}

int ProxyServer::port() const {
  // Doesn't make sense to call this if the URI scheme doesn't
  // have concept of a port.
  DCHECK(is_valid() && !is_direct());
  return port_;
}

std::string ProxyServer::host_and_port() const {
  // Doesn't make sense to call this if the URI scheme doesn't
  // have concept of a host.
  DCHECK(is_valid() && !is_direct());
  return host_ + ":" + IntToString(port_);
}

// static
ProxyServer ProxyServer::FromURI(const std::string& uri) {
  return FromURI(uri.begin(), uri.end());
}

// static
ProxyServer ProxyServer::FromURI(std::string::const_iterator begin,
                                 std::string::const_iterator end) {
  // We will default to HTTP if no scheme specifier was given.
  Scheme scheme = SCHEME_HTTP;

  // Trim the leading/trailing whitespace.
  HttpUtil::TrimLWS(&begin, &end);

  // Check for [<scheme> "://"]
  std::string::const_iterator colon = std::find(begin, end, ':');
  if (colon != end &&
      (end - colon) >= 3 &&
      *(colon + 1) == '/' &&
      *(colon + 2) == '/') {
    scheme = GetSchemeFromURI(begin, colon);
    begin = colon + 3;  // Skip past the "://"
  }

  // Now parse the <host>[":"<port>].
  return FromSchemeHostAndPort(scheme, begin, end);
}

std::string ProxyServer::ToURI() const {
  switch (scheme_) {
    case SCHEME_DIRECT:
      return "direct://";
    case SCHEME_HTTP:
      // Leave off "http://" since it is our default scheme.
      return host_and_port();
    case SCHEME_SOCKS4:
      return std::string("socks4://") + host_and_port();
    case SCHEME_SOCKS5:
      return std::string("socks5://") + host_and_port();
    default:
      // Got called with an invalid scheme.
      NOTREACHED();
      return std::string();
  }
}

// static
ProxyServer ProxyServer::FromPacString(const std::string& pac_string) {
  return FromPacString(pac_string.begin(), pac_string.end());
}

ProxyServer ProxyServer::FromPacString(std::string::const_iterator begin,
                                       std::string::const_iterator end) {
  // Trim the leading/trailing whitespace.
  HttpUtil::TrimLWS(&begin, &end);

  // Input should match:
  // "DIRECT" | ( <type> 1*(LWS) <host-and-port> )

  // Start by finding the first space (if any).
  std::string::const_iterator space;
  for (space = begin; space != end; ++space) {
    if (HttpUtil::IsLWS(*space)) {
      break;
    }
  }

  // Everything to the left of the space is the scheme.
  Scheme scheme = GetSchemeFromPacType(begin, space);

  // And everything to the right of the space is the
  // <host>[":" <port>].
  return FromSchemeHostAndPort(scheme, space, end);
}

std::string ProxyServer::ToPacString() const {
    switch (scheme_) {
    case SCHEME_DIRECT:
      return "DIRECT";
    case SCHEME_HTTP:
      return std::string("PROXY ") + host_and_port();
    case SCHEME_SOCKS4:
      // For compatibility send SOCKS instead of SOCKS4.
      return std::string("SOCKS ") + host_and_port();
    case SCHEME_SOCKS5:
      return std::string("SOCKS5 ") + host_and_port();
    default:
      // Got called with an invalid scheme.
      NOTREACHED();
      return std::string();
  }
}

// static
int ProxyServer::GetDefaultPortForScheme(Scheme scheme) {
  switch (scheme) {
    case SCHEME_HTTP:
      return 80;
    case SCHEME_SOCKS4:
    case SCHEME_SOCKS5:
      return 1080;
    default:
      return -1;
  }
}

// static
ProxyServer ProxyServer::FromSchemeHostAndPort(
    Scheme scheme,
    std::string::const_iterator begin,
    std::string::const_iterator end) {

  // Trim leading/trailing space.
  HttpUtil::TrimLWS(&begin, &end);

  if (scheme == SCHEME_DIRECT && begin != end)
    return ProxyServer();  // Invalid -- DIRECT cannot have a host/port.

  std::string host;
  int port = -1;

  if (scheme != SCHEME_INVALID && scheme != SCHEME_DIRECT) {
    // If the scheme has a host/port, parse it.
    bool ok = net::ParseHostAndPort(begin, end, &host, &port);
    if (!ok)
      return ProxyServer();  // Invalid -- failed parsing <host>[":"<port>]
  }

  // Choose a default port number if none was given.
  if (port == -1)
    port = GetDefaultPortForScheme(scheme);

  return ProxyServer(scheme, host, port);
}

}  // namespace net
