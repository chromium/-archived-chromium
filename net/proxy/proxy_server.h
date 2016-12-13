// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_PROXY_SERVER_H_
#define NET_PROXY_PROXY_SERVER_H_

#include <string>

namespace net {

// ProxyServer encodes the {type, host, port} of a proxy server.
// ProxyServer is immutable.
class ProxyServer {
 public:
  // The type of proxy. These are defined as bit flags so they can be ORed
  // together to pass as the |scheme_bit_field| argument to
  // ProxyService::RemoveProxiesWithoutScheme().
  enum Scheme {
    SCHEME_INVALID = 1 << 0,
    SCHEME_DIRECT  = 1 << 1,
    SCHEME_HTTP    = 1 << 2,
    SCHEME_SOCKS4  = 1 << 3,
    SCHEME_SOCKS5  = 1 << 4,
  };

  // Default copy-constructor and assignment operator are OK!

  // Constructs an invalid ProxyServer.
  ProxyServer() : scheme_(SCHEME_INVALID), port_(-1) {}

  // If |host| is an IPv6 literal address, it must include the square
  // brackets.
  ProxyServer(Scheme scheme, const std::string& host, int port)
      : scheme_(scheme), host_(host), port_(port) {}

  bool is_valid() const { return scheme_ != SCHEME_INVALID; }

  // Gets the proxy's scheme (i.e. SOCKS4, SOCKS5, HTTP}
  Scheme scheme() const { return scheme_; }

  // Returns true if this ProxyServer is actually just a DIRECT connection.
  bool is_direct() const { return scheme_ == SCHEME_DIRECT; }

  // Returns true if this ProxyServer is an HTTP proxy.
  bool is_http() const { return scheme_ == SCHEME_HTTP; }

  // Returns true if this ProxyServer is a SOCKS proxy.
  bool is_socks() const {
    return scheme_ == SCHEME_SOCKS4 || scheme_ == SCHEME_SOCKS5;
  }

  // Gets the host portion of the proxy server. If the host portion is an
  // IPv6 literal address, the return value does not include the square
  // brackets ([]) used to separate it from the port portion.
  std::string HostNoBrackets() const;

  // Gets the port portion of the proxy server.
  int port() const;

  // Returns the <host>":"<port> string for the proxy server.
  std::string host_and_port() const;

  // Parse from an input with format:
  //   [<scheme>"://"]<server>[":"<port>]
  //
  // Both <scheme> and <port> are optional. If <scheme> is omitted, it will
  // be assumed as "http". If <port> is omitted, it will be assumed as
  // the default port for the chosen scheme (80 for "http", 1080 for "socks").
  //
  // If parsing fails the instance will be set to invalid.
  //
  // Examples:
  //   "foopy"            {scheme=HTTP, host="foopy", port=80}
  //   "socks4://foopy"   {scheme=SOCKS4, host="foopy", port=1080}
  //   "socks5://foopy"   {scheme=SOCKS5, host="foopy", port=1080}
  //   "http://foopy:17"  {scheme=HTTP, host="foopy", port=17}
  //   "direct://"        {scheme=DIRECT}
  //   "foopy:X"          INVALID -- bad port.
  static ProxyServer FromURI(const std::string& uri);
  static ProxyServer FromURI(std::string::const_iterator uri_begin,
                             std::string::const_iterator uri_end);

  // Format as a URI string. This does the reverse of FromURI.
  std::string ToURI() const;

  // Parses from a PAC string result.
  //
  // If <port> is omitted, it will be assumed as the default port for the
  // chosen scheme (80 for "http", 1080 for "socks").
  //
  // If parsing fails the instance will be set to invalid.
  //
  // Examples:
  //   "PROXY foopy:19"   {scheme=HTTP, host="foopy", port=19}
  //   "DIRECT"           {scheme=DIRECT}
  //   "SOCKS5 foopy"     {scheme=SOCKS5, host="foopy", port=1080}
  //   "BLAH xxx:xx"      INVALID
  static ProxyServer FromPacString(const std::string& pac_string);
  static ProxyServer FromPacString(std::string::const_iterator pac_string_begin,
                                   std::string::const_iterator pac_string_end);

  // Format as a PAC result entry. This does the reverse of FromPacString().
  std::string ToPacString() const;

  // Returns the default port number for a proxy server with the specified
  // scheme. Returns -1 if unknown.
  static int GetDefaultPortForScheme(Scheme scheme);

  bool operator==(const ProxyServer& other) const {
    return scheme_ == other.scheme_ &&
           host_ == other.host_ &&
           port_ == other.port_;
  }

 private:
  // Create a ProxyServer given a scheme, and host/port string. If parsing the
  // host/port string fails, the returned instance will be invalid.
  static ProxyServer FromSchemeHostAndPort(
      Scheme scheme,
      std::string::const_iterator host_and_port_begin,
      std::string::const_iterator host_and_port_end);

  Scheme scheme_;
  std::string host_;
  int port_;
};

}  // namespace net

#endif  // NET_PROXY_PROXY_SERVER_H_
