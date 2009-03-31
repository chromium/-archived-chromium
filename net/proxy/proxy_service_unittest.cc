// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/compiler_specific.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_errors.h"
#include "net/proxy/proxy_config_service.h"
#include "net/proxy/proxy_resolver.h"
#include "net/proxy/proxy_script_fetcher.h"
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
  MockProxyResolver() : net::ProxyResolver(true),
                        fail_get_proxy_for_url(false) {
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

// ResultFuture is a handle to get at the result from
// ProxyService::ResolveProxyForURL() that ran on another thread.
class ResultFuture : public base::RefCountedThreadSafe<ResultFuture> {
 public:
  // |service| is the ProxyService to issue requests on, and |io_message_loop|
  // is the message loop where ProxyService lives.
  ResultFuture(MessageLoop* io_message_loop,
               net::ProxyService* service)
       : io_message_loop_(io_message_loop),
         service_(service),
         request_(NULL),
         ALLOW_THIS_IN_INITIALIZER_LIST(
             callback_(this, &ResultFuture::OnCompletion)),
         completion_(true, false),
         cancelled_(false, false),
         started_(false, false),
         did_complete_(false) {
  }

  // Block until the request has completed, then return the result.
  int GetResultCode() {
    DCHECK(MessageLoop::current() != io_message_loop_);
    WaitUntilCompleted();
    return result_code_;
  }

  // Block until the request has completed, then return the result.
  const net::ProxyInfo& GetProxyInfo() {
    DCHECK(MessageLoop::current() != io_message_loop_);
    WaitUntilCompleted();
    return proxy_info_;
  }

  // Cancel this request (wait until the cancel has been issued before
  // returning).
  void Cancel() {
    DCHECK(MessageLoop::current() != io_message_loop_);
    io_message_loop_->PostTask(FROM_HERE, NewRunnableMethod(
        this, &ResultFuture::DoCancel));
    cancelled_.Wait();
  }

  // Return true if the request has already completed.
  bool IsCompleted() {
    DCHECK(MessageLoop::current() != io_message_loop_);
    return did_complete_;
  }

  // Wait until the ProxyService completes this request.
  void WaitUntilCompleted() {
    DCHECK(MessageLoop::current() != io_message_loop_);
    completion_.Wait();
    DCHECK(did_complete_);
  }

 private:
  friend class ProxyServiceWithFutures;

  typedef int (net::ProxyService::*RequestMethod)(const GURL&, net::ProxyInfo*,
      net::CompletionCallback*, net::ProxyService::PacRequest**);

  void StartResolve(const GURL& url) {
    StartRequest(url, &net::ProxyService::ResolveProxy);
  }

  // |proxy_info| is the *previous* result (that we are reconsidering).
  void StartReconsider(const GURL& url, const net::ProxyInfo& proxy_info) {
    proxy_info_ = proxy_info;
    StartRequest(url, &net::ProxyService::ReconsiderProxyAfterError);
  }

  // Start the request. Return once ProxyService::GetProxyForURL() or
  // ProxyService::ReconsiderProxyAfterError() returns.
  void StartRequest(const GURL& url, RequestMethod method) {
    DCHECK(MessageLoop::current() != io_message_loop_);
    io_message_loop_->PostTask(FROM_HERE, NewRunnableMethod(
        this, &ResultFuture::DoStartRequest, url, method));
    started_.Wait();
  }

  // Called on |io_message_loop_|.
  void DoStartRequest(const GURL& url, RequestMethod method) {
    DCHECK(MessageLoop::current() == io_message_loop_);
    int rv = (service_->*method)(url, &proxy_info_, &callback_, &request_);
    if (rv != net::ERR_IO_PENDING) {
      // Completed synchronously.
      OnCompletion(rv);
    }
    started_.Signal();
  }

  // Called on |io_message_loop_|.
  void DoCancel() {
    DCHECK(MessageLoop::current() == io_message_loop_);
    if (!did_complete_)
      service_->CancelPacRequest(request_);
    cancelled_.Signal();
  }

  // Called on |io_message_loop_|.
  void OnCompletion(int result) {
    DCHECK(MessageLoop::current() == io_message_loop_);
    DCHECK(!did_complete_);
    did_complete_ = true;
    result_code_ = result;
    request_ = NULL;
    completion_.Signal();
  }

  // The message loop where the ProxyService lives.
  MessageLoop* io_message_loop_;

  // The proxy service that started this request.
  net::ProxyService* service_;

  // The in-progress request.
  net::ProxyService::PacRequest* request_;

  net::CompletionCallbackImpl<ResultFuture> callback_;
  base::WaitableEvent completion_;
  base::WaitableEvent cancelled_;
  base::WaitableEvent started_;
  bool did_complete_;

  // Results from the request.
  int result_code_;
  net::ProxyInfo proxy_info_;
};

// Wraps a ProxyService running on its own IO thread.
class ProxyServiceWithFutures {
 public:
  ProxyServiceWithFutures(net::ProxyConfigService* config_service,
                          net::ProxyResolver* resolver)
      : io_thread_("IO_Thread"),
        io_thread_state_(new IOThreadState) {
    base::Thread::Options options;
    options.message_loop_type = MessageLoop::TYPE_IO;
    io_thread_.StartWithOptions(options);

    // Initialize state that lives on |io_thread_|.
    io_thread_.message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
        io_thread_state_.get(), &IOThreadState::DoInit,
        config_service, resolver));
    io_thread_state_->event.Wait();
  }

  ~ProxyServiceWithFutures() {
    // Destroy state that lives on |io_thread_|.
    io_thread_.message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
        io_thread_state_.get(), &IOThreadState::DoDestroy));
    io_thread_state_->event.Wait();
  }

  // Start the request on |io_thread_|, and return a handle that can be
  // used to access the results. The caller is responsible for freeing
  // the ResultFuture.
  void ResolveProxy(scoped_refptr<ResultFuture>* result, const GURL& url) {
    *result = new ResultFuture(io_thread_.message_loop(),
                               io_thread_state_->service);
    (*result)->StartResolve(url);
  }

  // Same as above, but for "ReconsiderProxyAfterError()".
  void ReconsiderProxyAfterError(scoped_refptr<ResultFuture>* result,
                                 const GURL& url,
                                 const net::ProxyInfo& proxy_info) {
    *result = new ResultFuture(io_thread_.message_loop(),
                               io_thread_state_->service);
    (*result)->StartReconsider(url, proxy_info);
  }

  void SetProxyScriptFetcher(net::ProxyScriptFetcher* proxy_script_fetcher) {
    io_thread_.message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
        io_thread_state_.get(), &IOThreadState::DoSetProxyScriptFetcher,
        proxy_script_fetcher));
    io_thread_state_->event.Wait();
  }

 private:
  // Class that encapsulates the state living on IO thread. It needs to be
  // ref-counted for posting tasks.
  class IOThreadState : public base::RefCountedThreadSafe<IOThreadState> {
   public:
    IOThreadState() : event(false, false), service(NULL) {}

    void DoInit(net::ProxyConfigService* config_service,
                net::ProxyResolver* resolver) {
      service = new net::ProxyService(config_service, resolver);
      event.Signal();
    }

    void DoDestroy() {
      delete service;
      service = NULL;
      event.Signal();
    }

    void DoSetProxyScriptFetcher(
        net::ProxyScriptFetcher* proxy_script_fetcher) {
      service->SetProxyScriptFetcher(proxy_script_fetcher);
      event.Signal();
    }

    base::WaitableEvent event;
    net::ProxyService* service;
  };

  base::Thread io_thread_;
  scoped_refptr<IOThreadState> io_thread_state_;  // Lives on |io_thread_|.
};

// Wrapper around ProxyServiceWithFutures to do one request at a time.
class SyncProxyService {
 public:
  SyncProxyService(net::ProxyConfigService* config_service,
                   net::ProxyResolver* resolver)
      : service_(config_service, resolver) {
  }

  int ResolveProxy(const GURL& url, net::ProxyInfo* proxy_info) {
    scoped_refptr<ResultFuture> result;
    service_.ResolveProxy(&result, url);
    *proxy_info = result->GetProxyInfo();
    return result->GetResultCode();
  }

  int ReconsiderProxyAfterError(const GURL& url, net::ProxyInfo* proxy_info) {
    scoped_refptr<ResultFuture> result;
    service_.ReconsiderProxyAfterError(&result, url, *proxy_info);
    *proxy_info = result->GetProxyInfo();
    return result->GetResultCode();
  }

 private:
  ProxyServiceWithFutures service_;
};

// A ProxyResolver which can be set to block upon reaching GetProxyForURL.
class BlockableProxyResolver : public net::ProxyResolver {
 public:
  BlockableProxyResolver() : net::ProxyResolver(true),
                             should_block_(false),
                             unblocked_(true, true),
      blocked_(true, false) {
  }

  void Block() {
    should_block_ = true;
    unblocked_.Reset();
  }

  void Unblock() {
    should_block_ = false;
    blocked_.Reset();
    unblocked_.Signal();
  }

  void WaitUntilBlocked() {
    blocked_.Wait();
  }

  // net::ProxyResolver implementation:
  virtual int GetProxyForURL(const GURL& query_url,
                             const GURL& pac_url,
                             net::ProxyInfo* results) {
    if (should_block_) {
      blocked_.Signal();
      unblocked_.Wait();
    }

    results->UseNamedProxy(query_url.host());
    return net::OK;
  }

 private:
  bool should_block_;
  base::WaitableEvent unblocked_;
  base::WaitableEvent blocked_;
};

// A mock ProxyResolverWithoutFetch which concatenates the query's host with
// the last download PAC contents.  This way the result describes what the last
// downloaded PAC script's contents were, in addition to the query url itself.
class MockProxyResolverWithoutFetch : public net::ProxyResolver {
 public:
  MockProxyResolverWithoutFetch() : net::ProxyResolver(false),
                                    last_pac_contents_("NONE") {}

  // net::ProxyResolver implementation:
  virtual int GetProxyForURL(const GURL& query_url,
                             const GURL& pac_url,
                             net::ProxyInfo* results) {
    results->UseNamedProxy(last_pac_contents_ + "." + query_url.host());
    return net::OK;
  }

  virtual void SetPacScript(const std::string& bytes) {
    last_pac_contents_ = bytes;
  }

 private:
  std::string last_pac_contents_;
};

}  // namespace

// A mock ProxyScriptFetcher. No result will be returned to the fetch client
// until we call NotifyFetchCompletion() to set the results.
class MockProxyScriptFetcher : public net::ProxyScriptFetcher {
 public:
  MockProxyScriptFetcher() : pending_request_loop_(NULL),
      pending_request_callback_(NULL), pending_request_bytes_(NULL) {}

  // net::ProxyScriptFetcher implementation.
  virtual void Fetch(const GURL& url, std::string* bytes,
                     net::CompletionCallback* callback) {
    DCHECK(!HasPendingRequest());

    // Save the caller's information, and have them wait.
    pending_request_loop_ = MessageLoop::current();
    pending_request_url_ = url;
    pending_request_callback_ = callback;
    pending_request_bytes_ = bytes;
  }

  void NotifyFetchCompletion(int result, const std::string& bytes) {
    DCHECK(HasPendingRequest());
    pending_request_loop_->PostTask(FROM_HERE, NewRunnableMethod(
        this, &MockProxyScriptFetcher::DoNotifyFetchCompletion, result, bytes));
  }

  virtual void Cancel() {}

 private:
  // Runs on |pending_request_loop_|.
  void DoNotifyFetchCompletion(int result, const std::string& bytes) {
    DCHECK(HasPendingRequest());
    *pending_request_bytes_ = bytes;
    pending_request_callback_->Run(result);
  }

  bool HasPendingRequest() const {
    return pending_request_loop_ != NULL;
  }

  MessageLoop* pending_request_loop_;
  GURL pending_request_url_;
  net::CompletionCallback* pending_request_callback_;
  std::string* pending_request_bytes_;
};

// Template specialization so MockProxyScriptFetcher does not have to be
// refcounted.
template<>
void RunnableMethodTraits<MockProxyScriptFetcher>::RetainCallee(
    MockProxyScriptFetcher* remover) {}
template<>
void RunnableMethodTraits<MockProxyScriptFetcher>::ReleaseCallee(
    MockProxyScriptFetcher* remover) {}

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
  EXPECT_EQ("foopy:80", info.proxy_server().ToURI());
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
  EXPECT_EQ("foopy:8080", info.proxy_server().ToURI());

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
  EXPECT_EQ("foopy_valid:8080", info.proxy_server().ToURI());
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
  EXPECT_EQ("foopy1:8080", info.proxy_server().ToURI());

  // Fake an error on the proxy.
  rv = service.ReconsiderProxyAfterError(url, &info);
  EXPECT_EQ(rv, net::OK);

  // The second proxy should be specified.
  EXPECT_EQ("foopy2:9090", info.proxy_server().ToURI());

  // Create a new resolver that returns 3 proxies. The second one is already
  // known to be bad.
  config_service->config.pac_url = GURL("http://foopy/proxy.pac");
  resolver->info.UseNamedProxy("foopy3:7070;foopy1:8080;foopy2:9090");
  resolver->info_predicate_query_host = "www.google.com";
  resolver->fail_get_proxy_for_url = false;

  rv = service.ResolveProxy(url, &info);
  EXPECT_EQ(rv, net::OK);
  EXPECT_FALSE(info.is_direct());
  EXPECT_EQ("foopy3:7070", info.proxy_server().ToURI());

  // We fake another error. It should now try the third one.
  rv = service.ReconsiderProxyAfterError(url, &info);
  EXPECT_EQ(rv, net::OK);
  EXPECT_EQ("foopy2:9090", info.proxy_server().ToURI());

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
  EXPECT_EQ("foopy1:8080", info.proxy_server().ToURI());

  // Fake an error on the proxy, and also a new configuration on the proxy.
  config_service->config = net::ProxyConfig();
  config_service->config.pac_url = GURL("http://foopy-new/proxy.pac");

  rv = service.ReconsiderProxyAfterError(url, &info);
  EXPECT_EQ(rv, net::OK);

  // The first proxy is still there since the configuration changed.
  EXPECT_EQ("foopy1:8080", info.proxy_server().ToURI());

  // We fake another error. It should now ignore the first one.
  rv = service.ReconsiderProxyAfterError(url, &info);
  EXPECT_EQ(rv, net::OK);
  EXPECT_EQ("foopy2:9090", info.proxy_server().ToURI());

  // We simulate a new configuration.
  config_service->config = net::ProxyConfig();
  config_service->config.pac_url = GURL("http://foopy-new2/proxy.pac");

  // We fake anothe error. It should go back to the first proxy.
  rv = service.ReconsiderProxyAfterError(url, &info);
  EXPECT_EQ(rv, net::OK);
  EXPECT_EQ("foopy1:8080", info.proxy_server().ToURI());
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
  EXPECT_EQ("foopy1:8080", info.proxy_server().ToURI());

  // Fake a proxy error.
  rv = service.ReconsiderProxyAfterError(url, &info);
  EXPECT_EQ(rv, net::OK);

  // The first proxy is ignored, and the second one is selected.
  EXPECT_FALSE(info.is_direct());
  EXPECT_EQ("foopy2:9090", info.proxy_server().ToURI());

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
  EXPECT_EQ("foopy1:8080", info3.proxy_server().ToURI());
}

TEST(ProxyServiceTest, ProxyBypassList) {
  // Test what happens when a proxy bypass list is specified.

  net::ProxyConfig config;
  config.proxy_rules.ParseFromString("foopy1:8080;foopy2:9090");
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
  config.proxy_rules.ParseFromString("http=foopy1:8080;https=foopy2:8080");
  config.auto_detect = false;

  SyncProxyService service1(new MockProxyConfigService(config),
                            new MockProxyResolver);
  GURL test_url1("http://www.msn.com");
  net::ProxyInfo info1;
  int rv = service1.ResolveProxy(test_url1, &info1);
  EXPECT_EQ(rv, net::OK);
  EXPECT_FALSE(info1.is_direct());
  EXPECT_EQ("foopy1:8080", info1.proxy_server().ToURI());

  SyncProxyService service2(new MockProxyConfigService(config),
                            new MockProxyResolver);
  GURL test_url2("ftp://ftp.google.com");
  net::ProxyInfo info2;
  rv = service2.ResolveProxy(test_url2, &info2);
  EXPECT_EQ(rv, net::OK);
  EXPECT_TRUE(info2.is_direct());
  EXPECT_EQ("direct://", info2.proxy_server().ToURI());

  SyncProxyService service3(new MockProxyConfigService(config),
                            new MockProxyResolver);
  GURL test_url3("https://webbranch.techcu.com");
  net::ProxyInfo info3;
  rv = service3.ResolveProxy(test_url3, &info3);
  EXPECT_EQ(rv, net::OK);
  EXPECT_FALSE(info3.is_direct());
  EXPECT_EQ("foopy2:8080", info3.proxy_server().ToURI());

  config.proxy_rules.ParseFromString("foopy1:8080");
  SyncProxyService service4(new MockProxyConfigService(config),
                            new MockProxyResolver);
  GURL test_url4("www.microsoft.com");
  net::ProxyInfo info4;
  rv = service4.ResolveProxy(test_url4, &info4);
  EXPECT_EQ(rv, net::OK);
  EXPECT_FALSE(info4.is_direct());
  EXPECT_EQ("foopy1:8080", info4.proxy_server().ToURI());
}

// Test cancellation of a queued request.
TEST(ProxyServiceTest, CancelQueuedRequest) {
  MockProxyConfigService* config_service =
      new MockProxyConfigService("http://foopy/proxy.pac");

  BlockableProxyResolver* resolver = new BlockableProxyResolver;

  ProxyServiceWithFutures service(config_service, resolver);

  // Cause requests to pile up, by having them block in the PAC thread.
  resolver->Block();

  // Start 3 requests.
  scoped_refptr<ResultFuture> result1;
  service.ResolveProxy(&result1, GURL("http://request1"));

  scoped_refptr<ResultFuture> result2;
  service.ResolveProxy(&result2, GURL("http://request2"));

  scoped_refptr<ResultFuture> result3;
  service.ResolveProxy(&result3, GURL("http://request3"));

  // Wait until the first request has become blocked in the PAC thread.
  resolver->WaitUntilBlocked();

  // Cancel the second request
  result2->Cancel();

  // Unblock the PAC thread.
  resolver->Unblock();

  // Wait for the final request to complete.
  result3->WaitUntilCompleted();

  // Verify that requests ran as expected.

  EXPECT_TRUE(result1->IsCompleted());
  EXPECT_EQ(net::OK, result1->GetResultCode());
  EXPECT_EQ("request1:80", result1->GetProxyInfo().proxy_server().ToURI());

  EXPECT_FALSE(result2->IsCompleted());  // Cancelled.

  EXPECT_TRUE(result3->IsCompleted());
  EXPECT_EQ(net::OK, result3->GetResultCode());
  EXPECT_EQ("request3:80", result3->GetProxyInfo().proxy_server().ToURI());
}

// Test cancellation of an in-progress request.
TEST(ProxyServiceTest, CancelInprogressRequest) {
  MockProxyConfigService* config_service =
      new MockProxyConfigService("http://foopy/proxy.pac");

  BlockableProxyResolver* resolver = new BlockableProxyResolver;

  ProxyServiceWithFutures service(config_service, resolver);

  // Cause requests to pile up, by having them block in the PAC thread.
  resolver->Block();

  // Start 3 requests.
  scoped_refptr<ResultFuture> result1;
  service.ResolveProxy(&result1, GURL("http://request1"));

  scoped_refptr<ResultFuture> result2;
  service.ResolveProxy(&result2, GURL("http://request2"));

  scoped_refptr<ResultFuture> result3;
  service.ResolveProxy(&result3, GURL("http://request3"));

  // Wait until the first request has become blocked in the PAC thread.
  resolver->WaitUntilBlocked();

  // Cancel the first request
  result1->Cancel();

  // Unblock the PAC thread.
  resolver->Unblock();

  // Wait for the final request to complete.
  result3->WaitUntilCompleted();

  // Verify that requests ran as expected.

  EXPECT_FALSE(result1->IsCompleted());  // Cancelled.

  EXPECT_TRUE(result2->IsCompleted());
  EXPECT_EQ(net::OK, result2->GetResultCode());
  EXPECT_EQ("request2:80", result2->GetProxyInfo().proxy_server().ToURI());

  EXPECT_TRUE(result3->IsCompleted());
  EXPECT_EQ(net::OK, result3->GetResultCode());
  EXPECT_EQ("request3:80", result3->GetProxyInfo().proxy_server().ToURI());
}

// Test the initial PAC download for ProxyResolverWithoutFetch.
TEST(ProxyServiceTest, InitialPACScriptDownload) {
  MockProxyConfigService* config_service =
      new MockProxyConfigService("http://foopy/proxy.pac");

  MockProxyResolverWithoutFetch* resolver = new MockProxyResolverWithoutFetch;

  ProxyServiceWithFutures service(config_service, resolver);

  MockProxyScriptFetcher* fetcher = new MockProxyScriptFetcher;
  service.SetProxyScriptFetcher(fetcher);

  // Start 3 requests.
  scoped_refptr<ResultFuture> result1;
  service.ResolveProxy(&result1, GURL("http://request1"));

  scoped_refptr<ResultFuture> result2;
  service.ResolveProxy(&result2, GURL("http://request2"));

  scoped_refptr<ResultFuture> result3;
  service.ResolveProxy(&result3, GURL("http://request3"));

  // At this point the ProxyService should be waiting for the
  // ProxyScriptFetcher to invoke its completion callback, notifying it of
  // PAC script download completion.
  fetcher->NotifyFetchCompletion(net::OK, "pac-v1");

  // Complete all the requests.
  result3->WaitUntilCompleted();

  EXPECT_TRUE(result1->IsCompleted());
  EXPECT_EQ(net::OK, result1->GetResultCode());
  EXPECT_EQ("pac-v1.request1:80",
            result1->GetProxyInfo().proxy_server().ToURI());

  EXPECT_TRUE(result2->IsCompleted());
  EXPECT_EQ(net::OK, result2->GetResultCode());
  EXPECT_EQ("pac-v1.request2:80",
            result2->GetProxyInfo().proxy_server().ToURI());

  EXPECT_TRUE(result3->IsCompleted());
  EXPECT_EQ(net::OK, result3->GetResultCode());
  EXPECT_EQ("pac-v1.request3:80",
            result3->GetProxyInfo().proxy_server().ToURI());
}

// Test cancellation of a request, while the PAC script is being fetched.
TEST(ProxyServiceTest, CancelWhilePACFetching) {
  MockProxyConfigService* config_service =
      new MockProxyConfigService("http://foopy/proxy.pac");

  MockProxyResolverWithoutFetch* resolver = new MockProxyResolverWithoutFetch;

  ProxyServiceWithFutures service(config_service, resolver);

  MockProxyScriptFetcher* fetcher = new MockProxyScriptFetcher;
  service.SetProxyScriptFetcher(fetcher);

  // Start 3 requests.
  scoped_refptr<ResultFuture> result1;
  service.ResolveProxy(&result1, GURL("http://request1"));

  scoped_refptr<ResultFuture> result2;
  service.ResolveProxy(&result2, GURL("http://request2"));

  scoped_refptr<ResultFuture> result3;
  service.ResolveProxy(&result3, GURL("http://request3"));

  // Cancel the first 2 requests.
  result1->Cancel();
  result2->Cancel();

  // At this point the ProxyService should be waiting for the
  // ProxyScriptFetcher to invoke its completion callback, notifying it of
  // PAC script download completion.
  fetcher->NotifyFetchCompletion(net::OK, "pac-v1");

  // Complete all the requests.
  result3->WaitUntilCompleted();

  EXPECT_FALSE(result1->IsCompleted());  // Cancelled.
  EXPECT_FALSE(result2->IsCompleted());  // Cancelled.

  EXPECT_TRUE(result3->IsCompleted());
  EXPECT_EQ(net::OK, result3->GetResultCode());
  EXPECT_EQ("pac-v1.request3:80",
            result3->GetProxyInfo().proxy_server().ToURI());
}
