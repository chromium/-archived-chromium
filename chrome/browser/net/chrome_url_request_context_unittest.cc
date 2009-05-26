// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/common/chrome_switches.h"
#include "net/proxy/proxy_config.h"
#include "net/proxy/proxy_config_service_common_unittest.h"

#include "testing/gtest/include/gtest/gtest.h"

// Builds an identifier for each test in an array.
#define TEST_DESC(desc) StringPrintf("at line %d <%s>", __LINE__, desc)

TEST(ChromeUrlRequestContextTest, CreateProxyConfigTest) {
  // Build the input command lines here.
  CommandLine empty(L"foo.exe");
  CommandLine no_proxy(L"foo.exe");
  no_proxy.AppendSwitch(switches::kNoProxyServer);
  CommandLine no_proxy_extra_params(L"foo.exe");
  no_proxy_extra_params.AppendSwitch(switches::kNoProxyServer);
  no_proxy_extra_params.AppendSwitchWithValue(switches::kProxyServer,
      L"http://proxy:8888");
  CommandLine single_proxy(L"foo.exe");
  single_proxy.AppendSwitchWithValue(switches::kProxyServer,
      L"http://proxy:8888");
  CommandLine per_scheme_proxy(L"foo.exe");
  per_scheme_proxy.AppendSwitchWithValue(switches::kProxyServer,
      L"http=httpproxy:8888;ftp=ftpproxy:8889");
  CommandLine per_scheme_proxy_bypass(L"foo.exe");
  per_scheme_proxy_bypass.AppendSwitchWithValue(switches::kProxyServer,
      L"http=httpproxy:8888;ftp=ftpproxy:8889");
  per_scheme_proxy_bypass.AppendSwitchWithValue(
      switches::kProxyBypassList,
      L".google.com, foo.com:99, 1.2.3.4:22, 127.0.0.1/8");
  CommandLine with_pac_url(L"foo.exe");
  with_pac_url.AppendSwitchWithValue(switches::kProxyPacUrl,
      L"http://wpad/wpad.dat");
  with_pac_url.AppendSwitchWithValue(
      switches::kProxyBypassList,
      L".google.com, foo.com:99, 1.2.3.4:22, 127.0.0.1/8");
  CommandLine with_auto_detect(L"foo.exe");
  with_auto_detect.AppendSwitch(switches::kProxyAutoDetect);

  // Inspired from proxy_config_service_win_unittest.cc.
  const struct {
    // Short description to identify the test
    std::string description;

    // The command line to build a ProxyConfig from.
    const CommandLine& command_line;

    // Expected outputs (fields of the ProxyConfig).
    bool is_null;
    bool auto_detect;
    GURL pac_url;
    net::ProxyConfig::ProxyRules proxy_rules;
    const char* proxy_bypass_list;  // newline separated
    bool bypass_local_names;
  } tests[] = {
    {
      TEST_DESC("Empty command line"),
      // Input
      empty,
      // Expected result
      true,                                               // is_null
      false,                                              // auto_detect
      GURL(),                                             // pac_url
      net::ProxyConfig::ProxyRules(),                     // proxy_rules
      "",                                                 // proxy_bypass_list
      false                                               // bypass_local_names
    },
    {
      TEST_DESC("No proxy"),
      // Input
      no_proxy,
      // Expected result
      false,                                              // is_null
      false,                                              // auto_detect
      GURL(),                                             // pac_url
      net::ProxyConfig::ProxyRules(),                     // proxy_rules
      "",                                                 // proxy_bypass_list
      false                                               // bypass_local_names
    },
    {
      TEST_DESC("No proxy with extra parameters."),
      // Input
      no_proxy_extra_params,
      // Expected result
      false,                                              // is_null
      false,                                              // auto_detect
      GURL(),                                             // pac_url
      net::ProxyConfig::ProxyRules(),                     // proxy_rules
      "",                                                 // proxy_bypass_list
      false                                               // bypass_local_names
    },
    {
      TEST_DESC("Single proxy."),
      // Input
      single_proxy,
      // Expected result
      false,                                              // is_null
      false,                                              // auto_detect
      GURL(),                                             // pac_url
      net::MakeSingleProxyRules("http://proxy:8888"),     // proxy_rules
      "",                                                 // proxy_bypass_list
      false                                               // bypass_local_names
    },
    {
      TEST_DESC("Per scheme proxy."),
      // Input
      per_scheme_proxy,
      // Expected result
      false,                                              // is_null
      false,                                              // auto_detect
      GURL(),                                             // pac_url
      net::MakeProxyPerSchemeRules("httpproxy:8888",
                                   "",
                                   "ftpproxy:8889"),      // proxy_rules
      "",                                                 // proxy_bypass_list
      false                                               // bypass_local_names
    },
    {
      TEST_DESC("Per scheme proxy with bypass URLs."),
      // Input
      per_scheme_proxy_bypass,
      // Expected result
      false,                                              // is_null
      false,                                              // auto_detect
      GURL(),                                             // pac_url
      net::MakeProxyPerSchemeRules("httpproxy:8888",
                                   "",
                                   "ftpproxy:8889"),      // proxy_rules
      "*.google.com\n*foo.com:99\n1.2.3.4:22\n127.0.0.1/8\n",
      false                                               // bypass_local_names
    },
    {
      TEST_DESC("Pac URL with proxy bypass URLs"),
      // Input
      with_pac_url,
      // Expected result
      false,                                              // is_null
      false,                                              // auto_detect
      GURL("http://wpad/wpad.dat"),                       // pac_url
      net::ProxyConfig::ProxyRules(),                     // proxy_rules
      "*.google.com\n*foo.com:99\n1.2.3.4:22\n127.0.0.1/8\n",
      false                                               // bypass_local_names
    },
    {
      TEST_DESC("Autodetect"),
      // Input
      with_auto_detect,
      // Expected result
      false,                                              // is_null
      true,                                               // auto_detect
      GURL(),                                             // pac_url
      net::ProxyConfig::ProxyRules(),                     // proxy_rules
      "",                                                 // proxy_bypass_list
      false                                               // bypass_local_names
    }
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); i++) {
    SCOPED_TRACE(StringPrintf("Test[%d] %s", i, tests[i].description.c_str()));
    scoped_ptr<net::ProxyConfig> config(CreateProxyConfig(
        CommandLine(tests[i].command_line)));

    if (tests[i].is_null) {
      EXPECT_TRUE(config == NULL);
    } else {
      EXPECT_TRUE(config != NULL);
      EXPECT_EQ(tests[i].auto_detect, config->auto_detect);
      EXPECT_EQ(tests[i].pac_url, config->pac_url);
      EXPECT_EQ(tests[i].proxy_bypass_list,
                net::FlattenProxyBypass(config->proxy_bypass));
      EXPECT_EQ(tests[i].bypass_local_names, config->proxy_bypass_local_names);
      EXPECT_EQ(tests[i].proxy_rules, config->proxy_rules);
    }
  }
}
