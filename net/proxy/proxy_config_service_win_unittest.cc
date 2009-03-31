// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/proxy_config_service_win.h"

#include "net/base/net_errors.h"
#include "net/proxy/proxy_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

static ProxyConfig::ProxyRules MakeProxyRules(
    ProxyConfig::ProxyRules::Type type,
    const char* single_proxy,
    const char* proxy_for_http,
    const char* proxy_for_https,
    const char* proxy_for_ftp) {
  ProxyConfig::ProxyRules rules;
  rules.type = type;
  rules.single_proxy = ProxyServer::FromURI(single_proxy);
  rules.proxy_for_http = ProxyServer::FromURI(proxy_for_http);
  rules.proxy_for_https = ProxyServer::FromURI(proxy_for_https);
  rules.proxy_for_ftp = ProxyServer::FromURI(proxy_for_ftp);
  return rules;
}

static ProxyConfig::ProxyRules MakeSingleProxyRules(const char* single_proxy) {
  return MakeProxyRules(ProxyConfig::ProxyRules::TYPE_SINGLE_PROXY,
                        single_proxy, "", "", "");
}

static ProxyConfig::ProxyRules MakeProxyPerSchemeRules(
    const char* proxy_http,
    const char* proxy_https,
    const char* proxy_ftp) {
  return MakeProxyRules(ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME,
                        "", proxy_http, proxy_https, proxy_ftp);
}

TEST(ProxyConfigServiceWinTest, SetFromIEConfig) {
  const struct {
    // Input.
    WINHTTP_CURRENT_USER_IE_PROXY_CONFIG ie_config;

    // Expected outputs (fields of the ProxyConfig).
    bool auto_detect;
    GURL pac_url;
    ProxyConfig::ProxyRules proxy_rules;
    const char* proxy_bypass_list;  // newline separated
    bool bypass_local_names;
  } tests[] = {
    // Auto detect.
    {
      { // Input.
        TRUE,  // fAutoDetect
        NULL,  // lpszAutoConfigUrl
        NULL,  // lpszProxy
        NULL,  // lpszProxyBypass
      },

      // Expected result.
      true,                       // auto_detect
      GURL(),                     // pac_url
      ProxyConfig::ProxyRules(),  // proxy_rules
      "",                         // proxy_bypass_list
      false,                      // bypass_local_names
    },

    // Valid PAC url
    {
      { // Input.
        FALSE,                    // fAutoDetect
        L"http://wpad/wpad.dat",  // lpszAutoConfigUrl
        NULL,                     // lpszProxy
        NULL,                     // lpszProxy_bypass
      },

      // Expected result.
      false,                         // auto_detect
      GURL("http://wpad/wpad.dat"),  // pac_url
      ProxyConfig::ProxyRules(),     // proxy_rules
      "",                            // proxy_bypass_list
      false,                         // bypass_local_names
    },

    // Invalid PAC url string.
    {
      { // Input.
        FALSE,        // fAutoDetect
        L"wpad.dat",  // lpszAutoConfigUrl
        NULL,         // lpszProxy
        NULL,         // lpszProxy_bypass
      },

      // Expected result.
      false,                      // auto_detect
      GURL(),                     // pac_url
      ProxyConfig::ProxyRules(),  // proxy_rules
      "",                         // proxy_bypass_list
      false,                      // bypass_local_names
    },

    // Single-host in proxy list.
    {
      { // Input.
        FALSE,              // fAutoDetect
        NULL,               // lpszAutoConfigUrl
        L"www.google.com",  // lpszProxy
        NULL,               // lpszProxy_bypass
      },

      // Expected result.
      false,                                   // auto_detect
      GURL(),                                  // pac_url
      MakeSingleProxyRules("www.google.com"),  // proxy_rules
      "",                                      // proxy_bypass_list
      false,                                   // bypass_local_names
    },

    // Per-scheme proxy rules.
    {
      { // Input.
        FALSE,                                            // fAutoDetect
        NULL,                                             // lpszAutoConfigUrl
        L"http=www.google.com:80;https=www.foo.com:110",  // lpszProxy
        NULL,                                             // lpszProxy_bypass
      },

      // Expected result.
      false,                                   // auto_detect
      GURL(),                                  // pac_url
      MakeProxyPerSchemeRules("www.google.com:80", "www.foo.com:110", ""),
      "",                                      // proxy_bypass_list
      false,                                   // bypass_local_names
    },

    // Bypass local names.
    {
      { // Input.
        TRUE,            // fAutoDetect
        NULL,            // lpszAutoConfigUrl
        NULL,            // lpszProxy
        L"<local>",      // lpszProxy_bypass
      },

      true,                       // auto_detect
      GURL(),                     // pac_url
      ProxyConfig::ProxyRules(),  // proxy_rules
      "",                         // proxy_bypass_list
      true,                       // bypass_local_names
    },

    // Bypass "google.com" and local names, using semicolon as delimeter
    // (ignoring white space).
    {
      { // Input.
        TRUE,                         // fAutoDetect
        NULL,                         // lpszAutoConfigUrl
        NULL,                         // lpszProxy
        L"<local> ; google.com",      // lpszProxy_bypass
      },

      // Expected result.
      true,                       // auto_detect
      GURL(),                     // pac_url
      ProxyConfig::ProxyRules(),  // proxy_rules
      "google.com\n",             // proxy_bypass_list
      true,                       // bypass_local_names
    },

    // Bypass "foo.com" and "google.com", using lines as delimeter.
    {
      { // Input.
        TRUE,                      // fAutoDetect
        NULL,                      // lpszAutoConfigUrl
        NULL,                      // lpszProxy
        L"foo.com\r\ngoogle.com",  // lpszProxy_bypass
      },

      // Expected result.
      true,                       // auto_detect
      GURL(),                     // pac_url
      ProxyConfig::ProxyRules(),  // proxy_rules
      "foo.com\ngoogle.com\n",    // proxy_bypass_list
      false,                      // bypass_local_names
    },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    ProxyConfig config;
    ProxyConfigServiceWin::SetFromIEConfig(&config, tests[i].ie_config);

    EXPECT_EQ(tests[i].auto_detect, config.auto_detect);
    EXPECT_EQ(tests[i].pac_url, config.pac_url);

    // Join the proxy bypass list using "\n" to make it into a single string.
    std::string flattened_proxy_bypass;
    typedef std::vector<std::string> BypassList;
    for (BypassList::const_iterator it = config.proxy_bypass.begin();
         it != config.proxy_bypass.end(); ++it) {
      flattened_proxy_bypass += *it + "\n";
    }

    EXPECT_EQ(tests[i].proxy_bypass_list, flattened_proxy_bypass);
    EXPECT_EQ(tests[i].bypass_local_names, config.proxy_bypass_local_names);
    EXPECT_TRUE(tests[i].proxy_rules == config.proxy_rules);
  }
}

}  // namespace net
