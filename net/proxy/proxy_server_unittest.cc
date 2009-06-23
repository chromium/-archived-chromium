// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "net/proxy/proxy_server.h"
#include "testing/gtest/include/gtest/gtest.h"

// Test the creation of ProxyServer using ProxyServer::FromURI, which parses
// inputs of the form [<scheme>"://"]<host>[":"<port>]. Verify that each part
// was labelled correctly, and the accessors all give the right data.
TEST(ProxyServerTest, FromURI) {
  const struct {
    const char* input_uri;
    const char* expected_uri;
    net::ProxyServer::Scheme expected_scheme;
    const char* expected_host;
    int expected_port;
    const char* expected_host_and_port;
    const char* expected_pac_string;
  } tests[] = {
    // HTTP proxy URIs:
    {
       "foopy:10",  // No scheme.
       "foopy:10",
       net::ProxyServer::SCHEME_HTTP,
       "foopy",
       10,
       "foopy:10",
       "PROXY foopy:10"
    },
    {
       "http://foopy",  // No port.
       "foopy:80",
       net::ProxyServer::SCHEME_HTTP,
       "foopy",
       80,
       "foopy:80",
       "PROXY foopy:80"
    },
    {
       "http://foopy:10",
       "foopy:10",
       net::ProxyServer::SCHEME_HTTP,
       "foopy",
       10,
       "foopy:10",
       "PROXY foopy:10"
    },

    // IPv6 HTTP proxy URIs:
    {
       "[FEDC:BA98:7654:3210:FEDC:BA98:7654:3210]:10",  // No scheme.
       "[FEDC:BA98:7654:3210:FEDC:BA98:7654:3210]:10",
       net::ProxyServer::SCHEME_HTTP,
       "FEDC:BA98:7654:3210:FEDC:BA98:7654:3210",
       10,
       "[FEDC:BA98:7654:3210:FEDC:BA98:7654:3210]:10",
       "PROXY [FEDC:BA98:7654:3210:FEDC:BA98:7654:3210]:10"
    },
    {
       "http://[3ffe:2a00:100:7031::1]",  // No port.
       "[3ffe:2a00:100:7031::1]:80",
       net::ProxyServer::SCHEME_HTTP,
       "3ffe:2a00:100:7031::1",
       80,
       "[3ffe:2a00:100:7031::1]:80",
       "PROXY [3ffe:2a00:100:7031::1]:80"
    },
    {
       "http://[::192.9.5.5]",
       "[::192.9.5.5]:80",
       net::ProxyServer::SCHEME_HTTP,
       "::192.9.5.5",
       80,
       "[::192.9.5.5]:80",
       "PROXY [::192.9.5.5]:80"
    },
    {
       "http://[::FFFF:129.144.52.38]:80",
       "[::FFFF:129.144.52.38]:80",
       net::ProxyServer::SCHEME_HTTP,
       "::FFFF:129.144.52.38",
       80,
       "[::FFFF:129.144.52.38]:80",
       "PROXY [::FFFF:129.144.52.38]:80"
    },

    // SOCKS4 proxy URIs:
    {
       "socks4://foopy",  // No port.
       "socks4://foopy:1080",
       net::ProxyServer::SCHEME_SOCKS4,
       "foopy",
       1080,
       "foopy:1080",
       "SOCKS foopy:1080"
    },
    {
       "socks4://foopy:10",
       "socks4://foopy:10",
       net::ProxyServer::SCHEME_SOCKS4,
       "foopy",
       10,
       "foopy:10",
       "SOCKS foopy:10"
    },

    // SOCKS5 proxy URIs
    {
       "socks5://foopy",  // No port.
       "socks5://foopy:1080",
       net::ProxyServer::SCHEME_SOCKS5,
       "foopy",
       1080,
       "foopy:1080",
       "SOCKS5 foopy:1080"
    },
    {
       "socks5://foopy:10",
       "socks5://foopy:10",
       net::ProxyServer::SCHEME_SOCKS5,
       "foopy",
       10,
       "foopy:10",
       "SOCKS5 foopy:10"
    },

    // SOCKS proxy URIs (should default to SOCKS4)
    {
       "socks://foopy",  // No port.
       "socks4://foopy:1080",
       net::ProxyServer::SCHEME_SOCKS4,
       "foopy",
       1080,
       "foopy:1080",
       "SOCKS foopy:1080"
    },
    {
       "socks://foopy:10",
       "socks4://foopy:10",
       net::ProxyServer::SCHEME_SOCKS4,
       "foopy",
       10,
       "foopy:10",
       "SOCKS foopy:10"
    },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    net::ProxyServer uri = net::ProxyServer::FromURI(tests[i].input_uri);
    EXPECT_TRUE(uri.is_valid());
    EXPECT_FALSE(uri.is_direct());
    EXPECT_EQ(tests[i].expected_uri, uri.ToURI());
    EXPECT_EQ(tests[i].expected_scheme, uri.scheme());
    EXPECT_EQ(tests[i].expected_host, uri.HostNoBrackets());
    EXPECT_EQ(tests[i].expected_port, uri.port());
    EXPECT_EQ(tests[i].expected_host_and_port, uri.host_and_port());
    EXPECT_EQ(tests[i].expected_pac_string, uri.ToPacString());
  }
}

TEST(ProxyServerTest, DefaultConstructor) {
  net::ProxyServer proxy_server;
  EXPECT_FALSE(proxy_server.is_valid());
}

// Test parsing of the special URI form "direct://". Analagous to the "DIRECT"
// entry in a PAC result.
TEST(ProxyServerTest, Direct) {
  net::ProxyServer uri = net::ProxyServer::FromURI("direct://");
  EXPECT_TRUE(uri.is_valid());
  EXPECT_TRUE(uri.is_direct());
  EXPECT_EQ("direct://", uri.ToURI());
  EXPECT_EQ("DIRECT", uri.ToPacString());
}

// Test parsing some invalid inputs.
TEST(ProxyServerTest, Invalid) {
  const char* tests[] = {
    "",
    "   ",
    "dddf:",   // not a valid port
    "dddd:d",  // not a valid port
    "http://",  // not a valid host/port.
    "direct://xyz",  // direct is not allowed a host/port.
    "http:/",  // ambiguous, but will fail because of bad port.
    "http:",  // ambiguous, but will fail because of bad port.
    "https://blah",  // "https" is not a valid proxy scheme.
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    net::ProxyServer uri = net::ProxyServer::FromURI(tests[i]);
    EXPECT_FALSE(uri.is_valid());
    EXPECT_FALSE(uri.is_direct());
    EXPECT_FALSE(uri.is_http());
    EXPECT_FALSE(uri.is_socks());
  }
}

// Test that LWS (SP | HT) is disregarded from the ends.
TEST(ProxyServerTest, Whitespace) {
  const char* tests[] = {
    "  foopy:80",
    "foopy:80   \t",
    "  \tfoopy:80  ",
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    net::ProxyServer uri = net::ProxyServer::FromURI(tests[i]);
    EXPECT_EQ("foopy:80", uri.ToURI());
  }
}

// Test parsing a ProxyServer from a PAC representation.
TEST(ProxyServerTest, FromPACString) {
  const struct {
    const char* input_pac;
    const char* expected_uri;
  } tests[] = {
    {
       "PROXY foopy:10",
       "foopy:10",
    },
    {
       "   PROXY    foopy:10   ",
       "foopy:10",
    },
    {
       "pRoXy foopy:10",
       "foopy:10",
    },
    {
       "PROXY foopy",  // No port.
       "foopy:80",
    },
    {
       "socks foopy",
       "socks4://foopy:1080",
    },
    {
       "socks4 foopy",
       "socks4://foopy:1080",
    },
    {
       "socks5 foopy",
       "socks5://foopy:1080",
    },
    {
       "socks5 foopy:11",
       "socks5://foopy:11",
    },
    {
       " direct  ",
       "direct://",
    },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    net::ProxyServer uri = net::ProxyServer::FromPacString(tests[i].input_pac);
    EXPECT_TRUE(uri.is_valid());
    EXPECT_EQ(tests[i].expected_uri, uri.ToURI());
  }
}

// Test parsing a ProxyServer from an invalid PAC representation.
TEST(ProxyServerTest, FromPACStringInvalid) {
  const char* tests[] = {
    "PROXY",  // missing host/port.
    "SOCKS",  // missing host/port.
    "DIRECT foopy:10",  // direct cannot have host/port.
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    net::ProxyServer uri = net::ProxyServer::FromPacString(tests[i]);
    EXPECT_FALSE(uri.is_valid());
  }
}
