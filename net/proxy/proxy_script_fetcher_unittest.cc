// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/proxy_script_fetcher.h"

#include "base/file_path.h"
#include "base/compiler_specific.h"
#include "base/path_service.h"
#include "net/base/net_util.h"
#include "net/disk_cache/disk_cache.h"
#include "net/http/http_cache.h"
#include "net/url_request/url_request_unittest.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

// TODO(eroman):
//   - Test canceling an outstanding request.
//   - Test deleting ProxyScriptFetcher while a request is in progress.

const wchar_t kDocRoot[] = L"net/data/proxy_script_fetcher_unittest";

struct FetchResult {
  int code;
  std::string bytes;
};

// A non-mock URL request which can access http:// and file:// urls.
class RequestContext : public URLRequestContext {
 public:
  RequestContext() {
    net::ProxyConfig no_proxy;
    host_resolver_ = new net::HostResolver;
    proxy_service_ = net::ProxyService::CreateFixed(no_proxy);

    http_transaction_factory_ =
        new net::HttpCache(net::HttpNetworkLayer::CreateFactory(
            host_resolver_, proxy_service_),
            disk_cache::CreateInMemoryCacheBackend(0));
  }
  ~RequestContext() {
    delete http_transaction_factory_;
    delete proxy_service_;
  }
};

// Helper for doing synch fetches. This object lives in SynchFetcher's
// |io_thread_| and communicates with SynchFetcher though (|result|, |event|).
class SynchFetcherThreadHelper {
 public:
  SynchFetcherThreadHelper(base::WaitableEvent* event, FetchResult* result)
      : event_(event),
        fetch_result_(result),
        url_request_context_(NULL),
        fetcher_(NULL),
        ALLOW_THIS_IN_INITIALIZER_LIST(
            callback_(this, &SynchFetcherThreadHelper::OnFetchCompletion)) {
    url_request_context_ = new RequestContext;
    fetcher_.reset(net::ProxyScriptFetcher::Create(url_request_context_.get()));
  }

  // Starts fetching the script at |url|. Upon completion |event_| will be
  // signalled, and the bytes read will have been written to |fetch_result_|.
  void Start(const GURL& url) {
    fetcher_->Fetch(url, &fetch_result_->bytes, &callback_);
  }

  void OnFetchCompletion(int result) {
    fetch_result_->code = result;
    event_->Signal();
  }

 private:
  base::WaitableEvent* event_;
  FetchResult* fetch_result_;

  scoped_refptr<URLRequestContext> url_request_context_;

  scoped_ptr<net::ProxyScriptFetcher> fetcher_;
  net::CompletionCallbackImpl<SynchFetcherThreadHelper> callback_;
};

// Helper that wraps ProxyScriptFetcher::Fetch() with a synchronous interface.
// It executes Fetch() on a helper thread (IO_Thread).
class SynchFetcher {
 public:
  SynchFetcher()
      : event_(false, false),
        io_thread_("IO_Thread"),
        thread_helper_(NULL) {
    // Start an IO thread.
    base::Thread::Options options;
    options.message_loop_type = MessageLoop::TYPE_IO;
    io_thread_.StartWithOptions(options);

    // Initialize the state in |io_thread_|.
    io_thread_.message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
        this, &SynchFetcher::Init));
    Wait();
  }

  ~SynchFetcher() {
    // Tear down the state in |io_thread_|.
    io_thread_.message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
        this, &SynchFetcher::Cleanup));
    Wait();
  }

  // Synchronously fetch the url.
  FetchResult Fetch(const GURL& url) {
    io_thread_.message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
        this, &SynchFetcher::AsynchFetch, url));
    Wait();
    return fetch_result_;
  }

 private:
  // [Runs on |io_thread_|] Allocates the URLRequestContext and the
  // ProxyScriptFetcher, which live inside |thread_helper_|.
  void Init() {
    thread_helper_ = new SynchFetcherThreadHelper(&event_, &fetch_result_);
    event_.Signal();
  }

  // [Runs on |io_thread_|] Signals |event_| on completion.
  void AsynchFetch(const GURL& url) {
    thread_helper_->Start(url);
  }

  // [Runs on |io_thread_|] Signals |event_| on cleanup completion.
  void Cleanup() {
    delete thread_helper_;
    thread_helper_ = NULL;
    MessageLoop::current()->RunAllPending();
    event_.Signal();
  }

  void Wait() {
    event_.Wait();
    event_.Reset();
  }

  base::WaitableEvent event_;
  base::Thread io_thread_;
  FetchResult fetch_result_;
  // Holds all the state that lives on the IO thread, for easy cleanup.
  SynchFetcherThreadHelper* thread_helper_;
};

// Template specialization so SynchFetcher does not have to be refcounted.
template<>
void RunnableMethodTraits<SynchFetcher>::RetainCallee(SynchFetcher* remover) {}
template<>
void RunnableMethodTraits<SynchFetcher>::ReleaseCallee(SynchFetcher* remover) {}

// Required to be in net namespace by FRIEND_TEST.
namespace net {

// Get a file:// url relative to net/data/proxy/proxy_script_fetcher_unittest.
GURL GetTestFileUrl(const std::string& relpath) {
  FilePath path;
  PathService::Get(base::DIR_SOURCE_ROOT, &path);
  path = path.AppendASCII("net");
  path = path.AppendASCII("data");
  path = path.AppendASCII("proxy_script_fetcher_unittest");
  GURL base_url = net::FilePathToFileURL(path);
  return GURL(base_url.spec() + "/" + relpath);
}

typedef PlatformTest ProxyScriptFetcherTest;

TEST_F(ProxyScriptFetcherTest, FileUrl) {
  SynchFetcher pac_fetcher;

  { // Fetch a non-existent file.
    FetchResult result = pac_fetcher.Fetch(GetTestFileUrl("does-not-exist"));
    EXPECT_EQ(net::ERR_FILE_NOT_FOUND, result.code);
    EXPECT_TRUE(result.bytes.empty());
  }
  { // Fetch a file that exists.
    FetchResult result = pac_fetcher.Fetch(GetTestFileUrl("pac.txt"));
    EXPECT_EQ(net::OK, result.code);
    EXPECT_EQ("-pac.txt-\n", result.bytes);
  }
}

// Note that all mime types are allowed for PAC file, to be consistent
// with other browsers.
TEST_F(ProxyScriptFetcherTest, HttpMimeType) {
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(kDocRoot, NULL);
  ASSERT_TRUE(NULL != server.get());
  SynchFetcher pac_fetcher;

  { // Fetch a PAC with mime type "text/plain"
    GURL url = server->TestServerPage("files/pac.txt");
    FetchResult result = pac_fetcher.Fetch(url);
    EXPECT_EQ(net::OK, result.code);
    EXPECT_EQ("-pac.txt-\n", result.bytes);
  }
  { // Fetch a PAC with mime type "text/html"
    GURL url = server->TestServerPage("files/pac.html");
    FetchResult result = pac_fetcher.Fetch(url);
    EXPECT_EQ(net::OK, result.code);
    EXPECT_EQ("-pac.html-\n", result.bytes);
  }
  { // Fetch a PAC with mime type "application/x-ns-proxy-autoconfig"
    GURL url = server->TestServerPage("files/pac.nsproxy");
    FetchResult result = pac_fetcher.Fetch(url);
    EXPECT_EQ(net::OK, result.code);
    EXPECT_EQ("-pac.nsproxy-\n", result.bytes);
  }
}

TEST_F(ProxyScriptFetcherTest, HttpStatusCode) {
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(kDocRoot, NULL);
  ASSERT_TRUE(NULL != server.get());
  SynchFetcher pac_fetcher;

  { // Fetch a PAC which gives a 500 -- FAIL
    GURL url = server->TestServerPage("files/500.pac");
    FetchResult result = pac_fetcher.Fetch(url);
    EXPECT_EQ(net::ERR_PAC_STATUS_NOT_OK, result.code);
    EXPECT_TRUE(result.bytes.empty());
  }
  { // Fetch a PAC which gives a 404 -- FAIL
    GURL url = server->TestServerPage("files/404.pac");
    FetchResult result = pac_fetcher.Fetch(url);
    EXPECT_EQ(net::ERR_PAC_STATUS_NOT_OK, result.code);
    EXPECT_TRUE(result.bytes.empty());
  }
}

TEST_F(ProxyScriptFetcherTest, ContentDisposition) {
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(kDocRoot, NULL);
  ASSERT_TRUE(NULL != server.get());
  SynchFetcher pac_fetcher;

  // Fetch PAC scripts via HTTP with a Content-Disposition header -- should
  // have no effect.
  GURL url = server->TestServerPage("files/downloadable.pac");
  FetchResult result = pac_fetcher.Fetch(url);
  EXPECT_EQ(net::OK, result.code);
  EXPECT_EQ("-downloadable.pac-\n", result.bytes);
}

TEST_F(ProxyScriptFetcherTest, NoCache) {
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(kDocRoot, NULL);
  ASSERT_TRUE(NULL != server.get());
  SynchFetcher pac_fetcher;

  // Fetch a PAC script whose HTTP headers make it cacheable for 1 hour.
  GURL url = server->TestServerPage("files/cacheable_1hr.pac");
  FetchResult result = pac_fetcher.Fetch(url);
  EXPECT_EQ(net::OK, result.code);
  EXPECT_EQ("-cacheable_1hr.pac-\n", result.bytes);

  // Now kill the HTTP server.
  server->SendQuit();
  EXPECT_TRUE(server->WaitToFinish(20000));
  server = NULL;

  // Try to fetch the file again -- if should fail, since the server is not
  // running anymore. (If it were instead being loaded from cache, we would
  // get a success.
  result = pac_fetcher.Fetch(url);
  EXPECT_EQ(net::ERR_CONNECTION_REFUSED, result.code);
}

TEST_F(ProxyScriptFetcherTest, TooLarge) {
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(kDocRoot, NULL);
  ASSERT_TRUE(NULL != server.get());
  SynchFetcher pac_fetcher;

  // Set the maximum response size to 50 bytes.
  int prev_size = net::ProxyScriptFetcher::SetSizeConstraintForUnittest(50);

  // These two URLs are the same file, but are http:// vs file://
  GURL urls[] = {
    server->TestServerPage("files/large-pac.nsproxy"),
    GetTestFileUrl("large-pac.nsproxy")
  };

  // Try fetching URLs that are 101 bytes large. We should abort the request
  // after 50 bytes have been read, and fail with a too large error.
  for (size_t i = 0; i < arraysize(urls); ++i) {
    const GURL& url = urls[i];
    FetchResult result = pac_fetcher.Fetch(url);
    EXPECT_EQ(net::ERR_FILE_TOO_BIG, result.code);
    EXPECT_TRUE(result.bytes.empty());
  }

  // Restore the original size bound.
  net::ProxyScriptFetcher::SetSizeConstraintForUnittest(prev_size);

  { // Make sure we can still fetch regular URLs.
    GURL url = server->TestServerPage("files/pac.nsproxy");
    FetchResult result = pac_fetcher.Fetch(url);
    EXPECT_EQ(net::OK, result.code);
    EXPECT_EQ("-pac.nsproxy-\n", result.bytes);
  }
}

TEST_F(ProxyScriptFetcherTest, Hang) {
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(kDocRoot, NULL);
  ASSERT_TRUE(NULL != server.get());
  SynchFetcher pac_fetcher;

  // Set the timeout period to 0.5 seconds.
  int prev_timeout =
      net::ProxyScriptFetcher::SetTimeoutConstraintForUnittest(500);

  // Try fetching a URL which takes 1.2 seconds. We should abort the request
  // after 500 ms, and fail with a timeout error.
  { GURL url = server->TestServerPage("slow/proxy.pac?1.2");
    FetchResult result = pac_fetcher.Fetch(url);
    EXPECT_EQ(net::ERR_TIMED_OUT, result.code);
    EXPECT_TRUE(result.bytes.empty());
  }

  // Restore the original timeout period.
  net::ProxyScriptFetcher::SetTimeoutConstraintForUnittest(prev_timeout);

  { // Make sure we can still fetch regular URLs.
    GURL url = server->TestServerPage("files/pac.nsproxy");
    FetchResult result = pac_fetcher.Fetch(url);
    EXPECT_EQ(net::OK, result.code);
    EXPECT_EQ("-pac.nsproxy-\n", result.bytes);
  }
}

}  // namespace net
