// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "googleurl/src/gurl.h"
#include "net/base/net_errors.h"
#include "net/proxy/proxy_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockProxyResolver : public net::ProxyResolver {
 public:
  MockProxyResolver() : fail_get_proxy_for_url(false) {
    config.reset(new net::ProxyConfig);
  }
  virtual int GetProxyConfig(net::ProxyConfig* results) {
    *results = *(config.get());
    return net::OK;
  }
  virtual int GetProxyForURL(const std::string& query_url,
                             const std::string& pac_url,
                             net::ProxyInfo* results) {
    if (pac_url != config->pac_url)
      return net::ERR_INVALID_ARGUMENT;
    if (fail_get_proxy_for_url)
      return net::ERR_FAILED;
    if (GURL(query_url).host() == info_predicate_query_host) {
      results->Use(info);
    } else {
      results->UseDirect();
    }
    return net::OK;
  }
  scoped_ptr<net::ProxyConfig> config;
  net::ProxyInfo info;

  // info is only returned if query_url in GetProxyForURL matches this:
  std::string info_predicate_query_host;

  // If true, then GetProxyForURL will fail, which simulates failure to
  // download or execute the PAC file.
  bool fail_get_proxy_for_url;
};

}  // namespace

TEST(ProxyServiceTest, Direct) {
  MockProxyResolver resolver;
  net::ProxyService service(&resolver);

  GURL url("http://www.google.com/");

  net::ProxyInfo info;
  int rv = service.ResolveProxy(url, &info, NULL, NULL);
  EXPECT_EQ(rv, net::OK);
  EXPECT_TRUE(info.is_direct());
}

TEST(ProxyServiceTest, PAC) {
  MockProxyResolver resolver;
  resolver.config->pac_url = "http://foopy/proxy.pac";
  resolver.info.UseNamedProxy("foopy");
  resolver.info_predicate_query_host = "www.google.com";

  net::ProxyService service(&resolver);

  GURL url("http://www.google.com/");

  net::ProxyInfo info;
  int rv = service.ResolveProxy(url, &info, NULL, NULL);
  EXPECT_EQ(rv, net::OK);
  EXPECT_FALSE(info.is_direct());
  EXPECT_EQ(info.proxy_server(), "foopy");
}

TEST(ProxyServiceTest, PAC_FailoverToDirect) {
  MockProxyResolver resolver;
  resolver.config->pac_url = "http://foopy/proxy.pac";
  resolver.info.UseNamedProxy("foopy:8080");
  resolver.info_predicate_query_host = "www.google.com";

  net::ProxyService service(&resolver);

  GURL url("http://www.google.com/");

  net::ProxyInfo info;
  int rv = service.ResolveProxy(url, &info, NULL, NULL);
  EXPECT_EQ(rv, net::OK);
  EXPECT_FALSE(info.is_direct());
  EXPECT_EQ(info.proxy_server(), "foopy:8080");

  // Now, imagine that connecting to foopy:8080 fails.
  rv = service.ReconsiderProxyAfterError(url, &info, NULL, NULL);
  EXPECT_EQ(rv, net::OK);
  EXPECT_TRUE(info.is_direct());
}

TEST(ProxyServiceTest, PAC_FailsToDownload) {
  // Test what happens when we fail to download the PAC URL.

  MockProxyResolver resolver;
  resolver.config->pac_url = "http://foopy/proxy.pac";
  resolver.info.UseNamedProxy("foopy:8080");
  resolver.info_predicate_query_host = "www.google.com";
  resolver.fail_get_proxy_for_url = true;

  net::ProxyService service(&resolver);

  GURL url("http://www.google.com/");
  net::ProxyInfo info;
  int rv = service.ResolveProxy(url, &info, NULL, NULL);
  EXPECT_EQ(rv, net::OK);
  EXPECT_TRUE(info.is_direct());

  rv = service.ResolveProxy(url, &info, NULL, NULL);
  EXPECT_EQ(rv, net::OK);
  EXPECT_TRUE(info.is_direct());

  resolver.fail_get_proxy_for_url = false;
  resolver.info.UseNamedProxy("foopy_valid:8080");

  // But, if that fails, then we should give the proxy config another shot
  // since we have never tried it with this URL before.
  rv = service.ReconsiderProxyAfterError(url, &info, NULL, NULL);
  EXPECT_EQ(rv, net::OK);
  EXPECT_FALSE(info.is_direct());
  EXPECT_EQ(info.proxy_server(), "foopy_valid:8080");
}

TEST(ProxyServiceTest, ProxyFallback) {
  // Test what happens when we specify multiple proxy servers and some of them
  // are bad.

  MockProxyResolver resolver;
  resolver.config->pac_url = "http://foopy/proxy.pac";
  resolver.info.UseNamedProxy("foopy1:8080;foopy2:9090");
  resolver.info_predicate_query_host = "www.google.com";
  resolver.fail_get_proxy_for_url = false;

  net::ProxyService service(&resolver);

  GURL url("http://www.google.com/");

  // Get the proxy information.
  net::ProxyInfo info;
  int rv = service.ResolveProxy(url, &info, NULL, NULL);
  EXPECT_EQ(rv, net::OK);
  EXPECT_FALSE(info.is_direct());

  // The first item is valid.
  EXPECT_EQ(info.proxy_server(), "foopy1:8080");

  // Fake an error on the proxy.
  rv = service.ReconsiderProxyAfterError(url, &info, NULL, NULL);
  EXPECT_EQ(rv, net::OK);

  // The second proxy should be specified.
  EXPECT_EQ(info.proxy_server(), "foopy2:9090");

  // Create a new resolver that returns 3 proxies. The second one is already
  // known to be bad.
  resolver.config->pac_url = "http://foopy/proxy.pac";
  resolver.info.UseNamedProxy("foopy3:7070;foopy1:8080;foopy2:9090");
  resolver.info_predicate_query_host = "www.google.com";
  resolver.fail_get_proxy_for_url = false;

  rv = service.ResolveProxy(url, &info, NULL, NULL);
  EXPECT_EQ(rv, net::OK);
  EXPECT_FALSE(info.is_direct());
  EXPECT_EQ(info.proxy_server(), "foopy3:7070");

  // We fake another error. It should now try the third one.
  rv = service.ReconsiderProxyAfterError(url, &info, NULL, NULL);
  EXPECT_EQ(rv, net::OK);
  EXPECT_EQ(info.proxy_server(), "foopy2:9090");

  // Fake another error, the last proxy is gone, the list should now be empty.
  rv = service.ReconsiderProxyAfterError(url, &info, NULL, NULL);
  EXPECT_EQ(rv, net::OK);  // We try direct.
  EXPECT_TRUE(info.is_direct());

  // If it fails again, we don't have anything else to try.
  rv = service.ReconsiderProxyAfterError(url, &info, NULL, NULL);
  EXPECT_EQ(rv, net::ERR_FAILED);  // We try direct.

  // TODO(nsylvain): Test that the proxy can be retried after the delay.
}

TEST(ProxyServiceTest, ProxyFallback_NewSettings) {
  // Test proxy failover when new settings are available.

  MockProxyResolver resolver;
  resolver.config->pac_url = "http://foopy/proxy.pac";
  resolver.info.UseNamedProxy("foopy1:8080;foopy2:9090");
  resolver.info_predicate_query_host = "www.google.com";
  resolver.fail_get_proxy_for_url = false;

  net::ProxyService service(&resolver);

  GURL url("http://www.google.com/");

  // Get the proxy information.
  net::ProxyInfo info;
  int rv = service.ResolveProxy(url, &info, NULL, NULL);
  EXPECT_EQ(rv, net::OK);
  EXPECT_FALSE(info.is_direct());

  // The first item is valid.
  EXPECT_EQ(info.proxy_server(), "foopy1:8080");

  // Fake an error on the proxy, and also a new configuration on the proxy.
  resolver.config.reset(new net::ProxyConfig);
  resolver.config->pac_url = "http://foopy-new/proxy.pac";

  rv = service.ReconsiderProxyAfterError(url, &info, NULL, NULL);
  EXPECT_EQ(rv, net::OK);

  // The first proxy is still there since the configuration changed.
  EXPECT_EQ(info.proxy_server(), "foopy1:8080");

  // We fake another error. It should now ignore the first one.
  rv = service.ReconsiderProxyAfterError(url, &info, NULL, NULL);
  EXPECT_EQ(rv, net::OK);
  EXPECT_EQ(info.proxy_server(), "foopy2:9090");

  // We simulate a new configuration.
  resolver.config.reset(new net::ProxyConfig);
  resolver.config->pac_url = "http://foopy-new2/proxy.pac";

  // We fake anothe error. It should go back to the first proxy.
  rv = service.ReconsiderProxyAfterError(url, &info, NULL, NULL);
  EXPECT_EQ(rv, net::OK);
  EXPECT_EQ(info.proxy_server(), "foopy1:8080");
}

TEST(ProxyServiceTest, ProxyFallback_BadConfig) {
  // Test proxy failover when the configuration is bad.

  MockProxyResolver resolver;
  resolver.config->pac_url = "http://foopy/proxy.pac";
  resolver.info.UseNamedProxy("foopy1:8080;foopy2:9090");
  resolver.info_predicate_query_host = "www.google.com";
  resolver.fail_get_proxy_for_url = false;

  net::ProxyService service(&resolver);

  GURL url("http://www.google.com/");

  // Get the proxy information.
  net::ProxyInfo info;
  int rv = service.ResolveProxy(url, &info, NULL, NULL);
  EXPECT_EQ(rv, net::OK);
  EXPECT_FALSE(info.is_direct());

  // The first item is valid.
  EXPECT_EQ(info.proxy_server(), "foopy1:8080");

  // Fake a proxy error.
  rv = service.ReconsiderProxyAfterError(url, &info, NULL, NULL);
  EXPECT_EQ(rv, net::OK);

  // The first proxy is ignored, and the second one is selected.
  EXPECT_FALSE(info.is_direct());
  EXPECT_EQ(info.proxy_server(), "foopy2:9090");

  // Fake a PAC failure.
  net::ProxyInfo info2;
  resolver.fail_get_proxy_for_url = true;
  rv = service.ResolveProxy(url, &info2, NULL, NULL);
  EXPECT_EQ(rv, net::OK);

  // No proxy servers are returned. It's a direct connection.
  EXPECT_TRUE(info2.is_direct());

  // The PAC is now fixed and will return a proxy server.
  // It should also clear the list of bad proxies.
  resolver.fail_get_proxy_for_url = false;

  // Try to resolve, it will still return "direct" because we have no reason
  // to check the config since everything works.
  net::ProxyInfo info3;
  rv = service.ResolveProxy(url, &info3, NULL, NULL);
  EXPECT_EQ(rv, net::OK);
  EXPECT_TRUE(info3.is_direct());

  // But if the direct connection fails, we check if the ProxyInfo tried to
  // resolve the proxy before, and if not (like in this case), we give the
  // PAC another try.
  rv = service.ReconsiderProxyAfterError(url, &info3, NULL, NULL);
  EXPECT_EQ(rv, net::OK);

  // The first proxy is still there since the list of bad proxies got cleared.
  EXPECT_FALSE(info3.is_direct());
  EXPECT_EQ(info3.proxy_server(), "foopy1:8080");
}

TEST(ProxyServiceTest, ProxyBypassList) {
  // Test what happens when a proxy bypass list is specified.

  MockProxyResolver resolver;
  resolver.config->proxy_server = "foopy1:8080;foopy2:9090";
  resolver.config->auto_detect = false;
  resolver.config->proxy_bypass = "<local>";

  net::ProxyService service(&resolver);
  GURL url("http://www.google.com/");
  // Get the proxy information.
  net::ProxyInfo info;
  int rv = service.ResolveProxy(url, &info, NULL, NULL);
  EXPECT_EQ(rv, net::OK);
  EXPECT_FALSE(info.is_direct());

  net::ProxyService service1(&resolver);
  GURL test_url1("local");
  net::ProxyInfo info1;
  rv = service1.ResolveProxy(test_url1, &info1, NULL, NULL);
  EXPECT_EQ(rv, net::OK);
  EXPECT_TRUE(info1.is_direct());

  resolver.config->proxy_bypass = "<local>;*.org";
  net::ProxyService service2(&resolver);
  GURL test_url2("http://www.webkit.org");
  net::ProxyInfo info2;
  rv = service2.ResolveProxy(test_url2, &info2, NULL, NULL);
  EXPECT_EQ(rv, net::OK);
  EXPECT_TRUE(info2.is_direct());

  resolver.config->proxy_bypass = "<local>;*.org;7*";
  net::ProxyService service3(&resolver);
  GURL test_url3("http://74.125.19.147");
  net::ProxyInfo info3;
  rv = service3.ResolveProxy(test_url3, &info3, NULL, NULL);
  EXPECT_EQ(rv, net::OK);
  EXPECT_TRUE(info3.is_direct());

  resolver.config->proxy_bypass = "<local>;*.org;";
  net::ProxyService service4(&resolver);
  GURL test_url4("http://www.msn.com");
  net::ProxyInfo info4;
  rv = service4.ResolveProxy(test_url4, &info4, NULL, NULL);
  EXPECT_EQ(rv, net::OK);
  EXPECT_FALSE(info4.is_direct());
}

TEST(ProxyServiceTest, PerProtocolProxyTests) {
  MockProxyResolver resolver;
  resolver.config->proxy_server = "http=foopy1:8080;https=foopy2:8080";
  resolver.config->auto_detect = false;

  net::ProxyService service1(&resolver);
  GURL test_url1("http://www.msn.com");
  net::ProxyInfo info1;
  int rv = service1.ResolveProxy(test_url1, &info1, NULL, NULL);
  EXPECT_EQ(rv, net::OK);
  EXPECT_FALSE(info1.is_direct());
  EXPECT_TRUE(info1.proxy_server() == "foopy1:8080");

  net::ProxyService service2(&resolver);
  GURL test_url2("ftp://ftp.google.com");
  net::ProxyInfo info2;
  rv = service2.ResolveProxy(test_url2, &info2, NULL, NULL);
  EXPECT_EQ(rv, net::OK);
  EXPECT_TRUE(info2.is_direct());
  EXPECT_TRUE(info2.proxy_server() == "");

  net::ProxyService service3(&resolver);
  GURL test_url3("https://webbranch.techcu.com");
  net::ProxyInfo info3;
  rv = service3.ResolveProxy(test_url3, &info3, NULL, NULL);
  EXPECT_EQ(rv, net::OK);
  EXPECT_FALSE(info3.is_direct());
  EXPECT_TRUE(info3.proxy_server() == "foopy2:8080");

  resolver.config->proxy_server = "foopy1:8080";
  net::ProxyService service4(&resolver);
  GURL test_url4("www.microsoft.com");
  net::ProxyInfo info4;
  rv = service4.ResolveProxy(test_url4, &info4, NULL, NULL);
  EXPECT_EQ(rv, net::OK);
  EXPECT_FALSE(info4.is_direct());
  EXPECT_TRUE(info4.proxy_server() == "foopy1:8080");
}

