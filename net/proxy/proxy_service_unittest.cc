// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "googleurl/src/gurl.h"
#include "net/base/net_errors.h"
#include "net/proxy/proxy_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockProxyConfigService: public net::ProxyConfigService {
 public:
  MockProxyConfigService() {}  // Direct connect.
  explicit MockProxyConfigService(const net::ProxyConfig& pc) : config(pc) {}
  explicit MockProxyConfigService(const std::string& pac_url) {
    config.pac_url = GURL(pac_url);
  }

  virtual int GetProxyConfig(net::ProxyConfig* results) {
    *results = config;
    return net::OK;
  }

  net::ProxyConfig config;
};

class MockProxyResolver : public net::ProxyResolver {
 public:
  MockProxyResolver() : fail_get_proxy_for_url(false) {
  }

  virtual int GetProxyForURL(const GURL& query_url,
                             const GURL& pac_url,
                             net::ProxyInfo* results) {
    if (fail_get_proxy_for_url)
      return net::ERR_FAILED;
    if (GURL(query_url).host() == info_predicate_query_host) {
      results->Use(info);
    } else {
      results->UseDirect();
    }
    return net::OK;
  }

  net::ProxyInfo info;

  // info is only returned if query_url in GetProxyForURL matches this:
  std::string info_predicate_query_host;

  // If true, then GetProxyForURL will fail, which simulates failure to
  // download or execute the PAC file.
  bool fail_get_proxy_for_url;
};

class SyncProxyService {
 public:
  SyncProxyService(net::ProxyConfigService* config_service,
                   net::ProxyResolver* resolver)
      : io_thread_("IO_Thread"),
        service_(config_service, resolver) {
    base::Thread::Options options;
    options.message_loop_type = MessageLoop::TYPE_IO;
    io_thread_.StartWithOptions(options);
    sync_proxy_service_ = new net::SyncProxyServiceHelper(
        io_thread_.message_loop(), &service_);
  }

  int ResolveProxy(const GURL& url, net::ProxyInfo* proxy_info) {
    return sync_proxy_service_->ResolveProxy(url, proxy_info);
  }

  int ReconsiderProxyAfterError(const GURL& url, net::ProxyInfo* proxy_info) {
    return sync_proxy_service_->ReconsiderProxyAfterError(url, proxy_info);
  }

 private:
  base::Thread io_thread_;
  net::ProxyService service_;
  scoped_refptr<net::SyncProxyServiceHelper> sync_proxy_service_;
};

}  // namespace

// GetAnnotatedList() is used to generate a string for mozilla's GetProxyForUrl
// NPAPI extension. Check that it adheres to the expected format.
TEST(ProxyListTest, GetAnnotatedList) {
  net::ProxyList proxy_list;

  std::vector<std::string> proxies;
  proxies.push_back("www.first.com:80");
  proxies.push_back("www.second.com:80");
  proxy_list.SetVector(proxies);

  EXPECT_EQ(std::string("PROXY www.first.com:80;PROXY www.second.com:80"),
            proxy_list.GetAnnotatedList());
}

TEST(ProxyServiceTest, Direct) {
  SyncProxyService service(new MockProxyConfigService,
                           new MockProxyResolver);

  GURL url("http://www.google.com/");

  net::ProxyInfo info;
  int rv = service.ResolveProxy(url, &info);
  EXPECT_EQ(rv, net::OK);
  EXPECT_TRUE(info.is_direct());
}

TEST(ProxyServiceTest, PAC) {
  MockProxyConfigService* config_service =
      new MockProxyConfigService("http://foopy/proxy.pac");

  MockProxyResolver* resolver = new MockProxyResolver;
  resolver->info.UseNamedProxy("foopy");
  resolver->info_predicate_query_host = "www.google.com";

  SyncProxyService service(config_service, resolver);

  GURL url("http://www.google.com/");

  net::ProxyInfo info;
  int rv = service.ResolveProxy(url, &info);
  EXPECT_EQ(rv, net::OK);
  EXPECT_FALSE(info.is_direct());
  EXPECT_EQ(info.proxy_server(), "foopy");
}

TEST(ProxyServiceTest, PAC_FailoverToDirect) {
  MockProxyConfigService* config_service =
      new MockProxyConfigService("http://foopy/proxy.pac");

  MockProxyResolver* resolver = new MockProxyResolver;
  resolver->info.UseNamedProxy("foopy:8080");
  resolver->info_predicate_query_host = "www.google.com";

  SyncProxyService service(config_service, resolver);

  GURL url("http://www.google.com/");

  net::ProxyInfo info;
  int rv = service.ResolveProxy(url, &info);
  EXPECT_EQ(rv, net::OK);
  EXPECT_FALSE(info.is_direct());
  EXPECT_EQ(info.proxy_server(), "foopy:8080");

  // Now, imagine that connecting to foopy:8080 fails.
  rv = service.ReconsiderProxyAfterError(url, &info);
  EXPECT_EQ(rv, net::OK);
  EXPECT_TRUE(info.is_direct());
}

TEST(ProxyServiceTest, PAC_FailsToDownload) {
  // Test what happens when we fail to download the PAC URL.

  MockProxyConfigService* config_service =
      new MockProxyConfigService("http://foopy/proxy.pac");

  MockProxyResolver* resolver = new MockProxyResolver;
  resolver->info.UseNamedProxy("foopy:8080");
  resolver->info_predicate_query_host = "www.google.com";
  resolver->fail_get_proxy_for_url = true;

  SyncProxyService service(config_service, resolver);

  // The first resolve fails in the MockProxyResolver.
  GURL url("http://www.google.com/");
  net::ProxyInfo info;
  int rv = service.ResolveProxy(url, &info);
  EXPECT_EQ(rv, net::ERR_FAILED);

  // The second resolve request will automatically select direct connect,
  // because it has cached the configuration as being bad.
  rv = service.ResolveProxy(url, &info);
  EXPECT_EQ(rv, net::OK);
  EXPECT_TRUE(info.is_direct());

  resolver->fail_get_proxy_for_url = false;
  resolver->info.UseNamedProxy("foopy_valid:8080");

  // But, if that fails, then we should give the proxy config another shot
  // since we have never tried it with this URL before.
  rv = service.ReconsiderProxyAfterError(url, &info);
  EXPECT_EQ(rv, net::OK);
  EXPECT_FALSE(info.is_direct());
  EXPECT_EQ(info.proxy_server(), "foopy_valid:8080");
}

TEST(ProxyServiceTest, ProxyFallback) {
  // Test what happens when we specify multiple proxy servers and some of them
  // are bad.

  MockProxyConfigService* config_service =
      new MockProxyConfigService("http://foopy/proxy.pac");

  MockProxyResolver* resolver = new MockProxyResolver;
  resolver->info.UseNamedProxy("foopy1:8080;foopy2:9090");
  resolver->info_predicate_query_host = "www.google.com";
  resolver->fail_get_proxy_for_url = false;

  SyncProxyService service(config_service, resolver);

  GURL url("http://www.google.com/");

  // Get the proxy information.
  net::ProxyInfo info;
  int rv = service.ResolveProxy(url, &info);
  EXPECT_EQ(rv, net::OK);
  EXPECT_FALSE(info.is_direct());

  // The first item is valid.
  EXPECT_EQ(info.proxy_server(), "foopy1:8080");

  // Fake an error on the proxy.
  rv = service.ReconsiderProxyAfterError(url, &info);
  EXPECT_EQ(rv, net::OK);

  // The second proxy should be specified.
  EXPECT_EQ(info.proxy_server(), "foopy2:9090");

  // Create a new resolver that returns 3 proxies. The second one is already
  // known to be bad.
  config_service->config.pac_url = GURL("http://foopy/proxy.pac");
  resolver->info.UseNamedProxy("foopy3:7070;foopy1:8080;foopy2:9090");
  resolver->info_predicate_query_host = "www.google.com";
  resolver->fail_get_proxy_for_url = false;

  rv = service.ResolveProxy(url, &info);
  EXPECT_EQ(rv, net::OK);
  EXPECT_FALSE(info.is_direct());
  EXPECT_EQ(info.proxy_server(), "foopy3:7070");

  // We fake another error. It should now try the third one.
  rv = service.ReconsiderProxyAfterError(url, &info);
  EXPECT_EQ(rv, net::OK);
  EXPECT_EQ(info.proxy_server(), "foopy2:9090");

  // Fake another error, the last proxy is gone, the list should now be empty.
  rv = service.ReconsiderProxyAfterError(url, &info);
  EXPECT_EQ(rv, net::OK);  // We try direct.
  EXPECT_TRUE(info.is_direct());

  // If it fails again, we don't have anything else to try.
  rv = service.ReconsiderProxyAfterError(url, &info);
  EXPECT_EQ(rv, net::ERR_FAILED);  // We try direct.

  // TODO(nsylvain): Test that the proxy can be retried after the delay.
}

TEST(ProxyServiceTest, ProxyFallback_NewSettings) {
  // Test proxy failover when new settings are available.

  MockProxyConfigService* config_service =
      new MockProxyConfigService("http://foopy/proxy.pac");

  MockProxyResolver* resolver = new MockProxyResolver;
  resolver->info.UseNamedProxy("foopy1:8080;foopy2:9090");
  resolver->info_predicate_query_host = "www.google.com";
  resolver->fail_get_proxy_for_url = false;

  SyncProxyService service(config_service, resolver);

  GURL url("http://www.google.com/");

  // Get the proxy information.
  net::ProxyInfo info;
  int rv = service.ResolveProxy(url, &info);
  EXPECT_EQ(rv, net::OK);
  EXPECT_FALSE(info.is_direct());

  // The first item is valid.
  EXPECT_EQ(info.proxy_server(), "foopy1:8080");

  // Fake an error on the proxy, and also a new configuration on the proxy.
  config_service->config = net::ProxyConfig();
  config_service->config.pac_url = GURL("http://foopy-new/proxy.pac");

  rv = service.ReconsiderProxyAfterError(url, &info);
  EXPECT_EQ(rv, net::OK);

  // The first proxy is still there since the configuration changed.
  EXPECT_EQ(info.proxy_server(), "foopy1:8080");

  // We fake another error. It should now ignore the first one.
  rv = service.ReconsiderProxyAfterError(url, &info);
  EXPECT_EQ(rv, net::OK);
  EXPECT_EQ(info.proxy_server(), "foopy2:9090");

  // We simulate a new configuration.
  config_service->config = net::ProxyConfig();
  config_service->config.pac_url = GURL("http://foopy-new2/proxy.pac");

  // We fake anothe error. It should go back to the first proxy.
  rv = service.ReconsiderProxyAfterError(url, &info);
  EXPECT_EQ(rv, net::OK);
  EXPECT_EQ(info.proxy_server(), "foopy1:8080");
}

TEST(ProxyServiceTest, ProxyFallback_BadConfig) {
  // Test proxy failover when the configuration is bad.

  MockProxyConfigService* config_service =
      new MockProxyConfigService("http://foopy/proxy.pac");

  MockProxyResolver* resolver = new MockProxyResolver;
  resolver->info.UseNamedProxy("foopy1:8080;foopy2:9090");
  resolver->info_predicate_query_host = "www.google.com";
  resolver->fail_get_proxy_for_url = false;

  SyncProxyService service(config_service, resolver);

  GURL url("http://www.google.com/");

  // Get the proxy information.
  net::ProxyInfo info;
  int rv = service.ResolveProxy(url, &info);
  EXPECT_EQ(rv, net::OK);
  EXPECT_FALSE(info.is_direct());

  // The first item is valid.
  EXPECT_EQ(info.proxy_server(), "foopy1:8080");

  // Fake a proxy error.
  rv = service.ReconsiderProxyAfterError(url, &info);
  EXPECT_EQ(rv, net::OK);

  // The first proxy is ignored, and the second one is selected.
  EXPECT_FALSE(info.is_direct());
  EXPECT_EQ(info.proxy_server(), "foopy2:9090");

  // Fake a PAC failure.
  net::ProxyInfo info2;
  resolver->fail_get_proxy_for_url = true;
  rv = service.ResolveProxy(url, &info2);
  EXPECT_EQ(rv, net::ERR_FAILED);

  // No proxy servers are returned. It's a direct connection.
  EXPECT_TRUE(info2.is_direct());

  // The PAC is now fixed and will return a proxy server.
  // It should also clear the list of bad proxies.
  resolver->fail_get_proxy_for_url = false;

  // Try to resolve, it will still return "direct" because we have no reason
  // to check the config since everything works.
  net::ProxyInfo info3;
  rv = service.ResolveProxy(url, &info3);
  EXPECT_EQ(rv, net::OK);
  EXPECT_TRUE(info3.is_direct());

  // But if the direct connection fails, we check if the ProxyInfo tried to
  // resolve the proxy before, and if not (like in this case), we give the
  // PAC another try.
  rv = service.ReconsiderProxyAfterError(url, &info3);
  EXPECT_EQ(rv, net::OK);

  // The first proxy is still there since the list of bad proxies got cleared.
  EXPECT_FALSE(info3.is_direct());
  EXPECT_EQ(info3.proxy_server(), "foopy1:8080");
}

TEST(ProxyServiceTest, ProxyBypassList) {
  // Test what happens when a proxy bypass list is specified.

  net::ProxyConfig config;
  config.proxy_server = "foopy1:8080;foopy2:9090";
  config.auto_detect = false;
  config.proxy_bypass_local_names = true;
  
  SyncProxyService service(new MockProxyConfigService(config),
                           new MockProxyResolver());
  GURL url("http://www.google.com/");
  // Get the proxy information.
  net::ProxyInfo info;
  int rv = service.ResolveProxy(url, &info);
  EXPECT_EQ(rv, net::OK);
  EXPECT_FALSE(info.is_direct());

  SyncProxyService service1(new MockProxyConfigService(config),
                            new MockProxyResolver());
  GURL test_url1("local");
  net::ProxyInfo info1;
  rv = service1.ResolveProxy(test_url1, &info1);
  EXPECT_EQ(rv, net::OK);
  EXPECT_TRUE(info1.is_direct());

  config.proxy_bypass.clear();
  config.proxy_bypass.push_back("*.org");
  config.proxy_bypass_local_names = true;
  SyncProxyService service2(new MockProxyConfigService(config),
                            new MockProxyResolver);
  GURL test_url2("http://www.webkit.org");
  net::ProxyInfo info2;
  rv = service2.ResolveProxy(test_url2, &info2);
  EXPECT_EQ(rv, net::OK);
  EXPECT_TRUE(info2.is_direct());

  config.proxy_bypass.clear();
  config.proxy_bypass.push_back("*.org");
  config.proxy_bypass.push_back("7*");
  config.proxy_bypass_local_names = true;
  SyncProxyService service3(new MockProxyConfigService(config),
                            new MockProxyResolver);
  GURL test_url3("http://74.125.19.147");
  net::ProxyInfo info3;
  rv = service3.ResolveProxy(test_url3, &info3);
  EXPECT_EQ(rv, net::OK);
  EXPECT_TRUE(info3.is_direct());

  config.proxy_bypass.clear();
  config.proxy_bypass.push_back("*.org");
  config.proxy_bypass_local_names = true;
  SyncProxyService service4(new MockProxyConfigService(config),
                            new MockProxyResolver);
  GURL test_url4("http://www.msn.com");
  net::ProxyInfo info4;
  rv = service4.ResolveProxy(test_url4, &info4);
  EXPECT_EQ(rv, net::OK);
  EXPECT_FALSE(info4.is_direct());

  config.proxy_bypass.clear();
  config.proxy_bypass.push_back("*.MSN.COM");
  config.proxy_bypass_local_names = true;
  SyncProxyService service5(new MockProxyConfigService(config),
                            new MockProxyResolver);
  GURL test_url5("http://www.msnbc.msn.com");
  net::ProxyInfo info5;
  rv = service5.ResolveProxy(test_url5, &info5);
  EXPECT_EQ(rv, net::OK);
  EXPECT_TRUE(info5.is_direct());

  config.proxy_bypass.clear();
  config.proxy_bypass.push_back("*.msn.com");
  config.proxy_bypass_local_names = true;
  SyncProxyService service6(new MockProxyConfigService(config),
                            new MockProxyResolver);
  GURL test_url6("HTTP://WWW.MSNBC.MSN.COM");
  net::ProxyInfo info6;
  rv = service6.ResolveProxy(test_url6, &info6);
  EXPECT_EQ(rv, net::OK);
  EXPECT_TRUE(info6.is_direct());
}

TEST(ProxyServiceTest, PerProtocolProxyTests) {
  net::ProxyConfig config;
  config.proxy_server = "http=foopy1:8080;https=foopy2:8080";
  config.auto_detect = false;

  SyncProxyService service1(new MockProxyConfigService(config),
                            new MockProxyResolver);
  GURL test_url1("http://www.msn.com");
  net::ProxyInfo info1;
  int rv = service1.ResolveProxy(test_url1, &info1);
  EXPECT_EQ(rv, net::OK);
  EXPECT_FALSE(info1.is_direct());
  EXPECT_TRUE(info1.proxy_server() == "foopy1:8080");

  SyncProxyService service2(new MockProxyConfigService(config),
                            new MockProxyResolver);
  GURL test_url2("ftp://ftp.google.com");
  net::ProxyInfo info2;
  rv = service2.ResolveProxy(test_url2, &info2);
  EXPECT_EQ(rv, net::OK);
  EXPECT_TRUE(info2.is_direct());
  EXPECT_TRUE(info2.proxy_server() == "");

  SyncProxyService service3(new MockProxyConfigService(config),
                            new MockProxyResolver);
  GURL test_url3("https://webbranch.techcu.com");
  net::ProxyInfo info3;
  rv = service3.ResolveProxy(test_url3, &info3);
  EXPECT_EQ(rv, net::OK);
  EXPECT_FALSE(info3.is_direct());
  EXPECT_TRUE(info3.proxy_server() == "foopy2:8080");

  config.proxy_server = "foopy1:8080";
  SyncProxyService service4(new MockProxyConfigService(config),
                            new MockProxyResolver);
  GURL test_url4("www.microsoft.com");
  net::ProxyInfo info4;
  rv = service4.ResolveProxy(test_url4, &info4);
  EXPECT_EQ(rv, net::OK);
  EXPECT_FALSE(info4.is_direct());
  EXPECT_TRUE(info4.proxy_server() == "foopy1:8080");
}

