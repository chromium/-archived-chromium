// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/proxy_config.h"
#include "net/proxy/proxy_config_service_common_unittest.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
static void ExpectProxyServerEquals(const char* expectation,
                                    const net::ProxyServer& proxy_server) {
  if (expectation == NULL) {
    EXPECT_FALSE(proxy_server.is_valid());
  } else {
    EXPECT_EQ(expectation, proxy_server.ToURI());
  }
}
}

TEST(ProxyConfigTest, Equals) {
  // Test |ProxyConfig::auto_detect|.

  net::ProxyConfig config1;
  config1.auto_detect = true;

  net::ProxyConfig config2;
  config2.auto_detect = false;

  EXPECT_FALSE(config1.Equals(config2));
  EXPECT_FALSE(config2.Equals(config1));

  config2.auto_detect = true;

  EXPECT_TRUE(config1.Equals(config2));
  EXPECT_TRUE(config2.Equals(config1));

  // Test |ProxyConfig::pac_url|.

  config2.pac_url = GURL("http://wpad/wpad.dat");

  EXPECT_FALSE(config1.Equals(config2));
  EXPECT_FALSE(config2.Equals(config1));

  config1.pac_url = GURL("http://wpad/wpad.dat");

  EXPECT_TRUE(config1.Equals(config2));
  EXPECT_TRUE(config2.Equals(config1));

  // Test |ProxyConfig::proxy_rules|.

  config2.proxy_rules.type = net::ProxyConfig::ProxyRules::TYPE_SINGLE_PROXY;
  config2.proxy_rules.single_proxy = net::ProxyServer::FromURI("myproxy:80");

  EXPECT_FALSE(config1.Equals(config2));
  EXPECT_FALSE(config2.Equals(config1));

  config1.proxy_rules.type = net::ProxyConfig::ProxyRules::TYPE_SINGLE_PROXY;
  config1.proxy_rules.single_proxy = net::ProxyServer::FromURI("myproxy:100");

  EXPECT_FALSE(config1.Equals(config2));
  EXPECT_FALSE(config2.Equals(config1));

  config1.proxy_rules.single_proxy = net::ProxyServer::FromURI("myproxy");

  EXPECT_TRUE(config1.Equals(config2));
  EXPECT_TRUE(config2.Equals(config1));

  // Test |ProxyConfig::proxy_bypass|.

  config2.proxy_bypass.push_back("*.google.com");

  EXPECT_FALSE(config1.Equals(config2));
  EXPECT_FALSE(config2.Equals(config1));

  config1.proxy_bypass.push_back("*.google.com");

  EXPECT_TRUE(config1.Equals(config2));
  EXPECT_TRUE(config2.Equals(config1));

  // Test |ProxyConfig::proxy_bypass_local_names|.

  config1.proxy_bypass_local_names = true;

  EXPECT_FALSE(config1.Equals(config2));
  EXPECT_FALSE(config2.Equals(config1));

  config2.proxy_bypass_local_names = true;

  EXPECT_TRUE(config1.Equals(config2));
  EXPECT_TRUE(config2.Equals(config1));
}

TEST(ProxyConfigTest, ParseProxyRules) {
  const struct {
    const char* proxy_rules;

    net::ProxyConfig::ProxyRules::Type type;
    const char* single_proxy;
    const char* proxy_for_http;
    const char* proxy_for_https;
    const char* proxy_for_ftp;
  } tests[] = {
    // One HTTP proxy for all schemes.
    {
      "myproxy:80",

      net::ProxyConfig::ProxyRules::TYPE_SINGLE_PROXY,
      "myproxy:80",
      NULL,
      NULL,
      NULL,
    },

    // Only specify a proxy server for "http://" urls.
    {
      "http=myproxy:80",

      net::ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME,
      NULL,
      "myproxy:80",
      NULL,
      NULL,
    },

    // Specify an HTTP proxy for "ftp://" and a SOCKS proxy for "https://" urls.
    {
      "ftp=ftp-proxy ; https=socks4://foopy",

      net::ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME,
      NULL,
      NULL,
      "socks4://foopy:1080",
      "ftp-proxy:80",
    },

    // Give a scheme-specific proxy as well as a non-scheme specific.
    // The first entry "foopy" takes precedance marking this list as
    // TYPE_SINGLE_PROXY.
    {
      "foopy ; ftp=ftp-proxy",

      net::ProxyConfig::ProxyRules::TYPE_SINGLE_PROXY,
      "foopy:80",
      NULL,
      NULL,
      NULL,
    },

    // Give a scheme-specific proxy as well as a non-scheme specific.
    // The first entry "ftp=ftp-proxy" takes precedance marking this list as
    // TYPE_PROXY_PER_SCHEME.
    {
      "ftp=ftp-proxy ; foopy",

      net::ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME,
      NULL,
      NULL,
      NULL,
      "ftp-proxy:80",
    },

    // Include duplicate entries -- last one wins.
    {
      "ftp=ftp1 ; ftp=ftp2 ; ftp=ftp3",

      net::ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME,
      NULL,
      NULL,
      NULL,
      "ftp3:80",
    },

    // Only socks proxy present, others being blank.
    {  // NOLINT
      "socks=foopy",

      net::ProxyConfig::ProxyRules::TYPE_SINGLE_PROXY,
      "socks4://foopy:1080",
      NULL,
      NULL,
      NULL,
    },

    // Include unsupported schemes -- they are discarded.
    {
      "crazy=foopy ; foo=bar ; https=myhttpsproxy",

      net::ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME,
      NULL,
      NULL,
      "myhttpsproxy:80",
      NULL,
    },
  };

  net::ProxyConfig config;

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    config.proxy_rules.ParseFromString(tests[i].proxy_rules);

    EXPECT_EQ(tests[i].type, config.proxy_rules.type);
    ExpectProxyServerEquals(tests[i].single_proxy,
                            config.proxy_rules.single_proxy);
    ExpectProxyServerEquals(tests[i].proxy_for_http,
                            config.proxy_rules.proxy_for_http);
    ExpectProxyServerEquals(tests[i].proxy_for_https,
                            config.proxy_rules.proxy_for_https);
    ExpectProxyServerEquals(tests[i].proxy_for_ftp,
                            config.proxy_rules.proxy_for_ftp);
  }
}

TEST(ProxyConfigTest, ParseProxyBypassList) {
  struct bypass_test {
    const char* proxy_bypass_input;
    const char* flattened_output;
  };

  const struct {
    const char* proxy_bypass_input;
    const char* flattened_output;
  } tests[] = {
    {
      "*",
      "*\n"
    },
    {
      ".google.com, .foo.com:42",
      "*.google.com\n*.foo.com:42\n"
    },
    {
      ".google.com, foo.com:99, 1.2.3.4:22, 127.0.0.1/8",
      "*.google.com\n*foo.com:99\n1.2.3.4:22\n127.0.0.1/8\n"
    }
  };

  net::ProxyConfig config;

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    config.ParseNoProxyList(tests[i].proxy_bypass_input);
    EXPECT_EQ(tests[i].flattened_output,
              net::FlattenProxyBypass(config.proxy_bypass));
  }
}
