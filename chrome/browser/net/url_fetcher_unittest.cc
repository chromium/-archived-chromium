// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/thread.h"
#include "base/time.h"
#include "chrome/browser/net/url_fetcher.h"
#include "chrome/browser/net/url_fetcher_protect.h"
#if defined(OS_LINUX)
// TODO(port): ugly hack for linux
namespace ChromePluginLib { 
	void UnloadAllPlugins() {} 
}
#else
#include "chrome/common/chrome_plugin_lib.h"
#endif
#include "net/base/ssl_test_util.h"
#include "net/url_request/url_request_unittest.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;
using base::TimeDelta;

namespace {

const wchar_t kDocRoot[] = L"chrome/test/data";

class URLFetcherTest : public testing::Test, public URLFetcher::Delegate {
 public:
  URLFetcherTest() : fetcher_(NULL) { }

  // Creates a URLFetcher, using the program's main thread to do IO.
  virtual void CreateFetcher(const GURL& url);

  // URLFetcher::Delegate
  virtual void OnURLFetchComplete(const URLFetcher* source,
                                  const GURL& url,
                                  const URLRequestStatus& status,
                                  int response_code,
                                  const ResponseCookies& cookies,
                                  const std::string& data);

 protected:
  virtual void SetUp() {
    testing::Test::SetUp();

    // Ensure that any plugin operations done by other tests are cleaned up.
    ChromePluginLib::UnloadAllPlugins();
  }

  // URLFetcher is designed to run on the main UI thread, but in our tests
  // we assume that the current thread is the IO thread where the URLFetcher
  // dispatches its requests to.  When we wish to simulate being used from
  // a UI thread, we dispatch a worker thread to do so.
  MessageLoopForIO io_loop_;

  URLFetcher* fetcher_;
};

// Version of URLFetcherTest that does a POST instead
class URLFetcherPostTest : public URLFetcherTest {
 public:
  virtual void CreateFetcher(const GURL& url);

  // URLFetcher::Delegate
  virtual void OnURLFetchComplete(const URLFetcher* source,
                                  const GURL& url,
                                  const URLRequestStatus& status,
                                  int response_code,
                                  const ResponseCookies& cookies,
                                  const std::string& data);
};

// Version of URLFetcherTest that tests headers.
class URLFetcherHeadersTest : public URLFetcherTest {
 public:
  // URLFetcher::Delegate
  virtual void OnURLFetchComplete(const URLFetcher* source,
                                  const GURL& url,
                                  const URLRequestStatus& status,
                                  int response_code,
                                  const ResponseCookies& cookies,
                                  const std::string& data);
};

// Version of URLFetcherTest that tests overload proctection.
class URLFetcherProtectTest : public URLFetcherTest {
 public:
  virtual void CreateFetcher(const GURL& url);
  // URLFetcher::Delegate
  virtual void OnURLFetchComplete(const URLFetcher* source,
                                  const GURL& url,
                                  const URLRequestStatus& status,
                                  int response_code,
                                  const ResponseCookies& cookies,
                                  const std::string& data);
 private:
  Time start_time_;
};

// Version of URLFetcherTest that tests bad HTTPS requests.
class URLFetcherBadHTTPSTest : public URLFetcherTest {
 public:
  URLFetcherBadHTTPSTest();

  // URLFetcher::Delegate
  virtual void OnURLFetchComplete(const URLFetcher* source,
                                  const GURL& url,
                                  const URLRequestStatus& status,
                                  int response_code,
                                  const ResponseCookies& cookies,
                                  const std::string& data);

 protected:
  FilePath GetExpiredCertPath();
  SSLTestUtil util_;

 private:
  FilePath cert_dir_;
};

// Version of URLFetcherTest that tests request cancellation on shutdown.
class URLFetcherCancelTest : public URLFetcherTest {
 public:
  virtual void CreateFetcher(const GURL& url);
  // URLFetcher::Delegate
  virtual void OnURLFetchComplete(const URLFetcher* source,
                                  const GURL& url,
                                  const URLRequestStatus& status,
                                  int response_code,
                                  const ResponseCookies& cookies,
                                  const std::string& data);

  void CancelRequest();
  void TestContextReleased();

 private:
  base::OneShotTimer<URLFetcherCancelTest> timer_;
  bool context_released_;
};

// Version of TestURLRequestContext that let us know if the request context
// is properly released.
class CancelTestURLRequestContext : public TestURLRequestContext {
 public:
  explicit CancelTestURLRequestContext(bool* destructor_called)
      : destructor_called_(destructor_called) {
    *destructor_called_ = false;
  }

  virtual ~CancelTestURLRequestContext() {
    *destructor_called_ = true;
  }

 private:
  bool* destructor_called_;
};

// Wrapper that lets us call CreateFetcher() on a thread of our choice.  We
// could make URLFetcherTest refcounted and use PostTask(FROM_HERE.. ) to call
// CreateFetcher() directly, but the ownership of the URLFetcherTest is a bit
// confusing in that case because GTest doesn't know about the refcounting.
// It's less confusing to just do it this way.
class FetcherWrapperTask : public Task {
 public:
  FetcherWrapperTask(URLFetcherTest* test, const GURL& url)
      : test_(test), url_(url) { }
  virtual void Run() {
    test_->CreateFetcher(url_);
  };

 private:
  URLFetcherTest* test_;
  GURL url_;
};

void URLFetcherTest::CreateFetcher(const GURL& url) {
  fetcher_ = new URLFetcher(url, URLFetcher::GET, this);
  fetcher_->set_request_context(new TestURLRequestContext());
  fetcher_->set_io_loop(&io_loop_);
  fetcher_->Start();
}

void URLFetcherTest::OnURLFetchComplete(const URLFetcher* source,
                                        const GURL& url,
                                        const URLRequestStatus& status,
                                        int response_code,
                                        const ResponseCookies& cookies,
                                        const std::string& data) {
  EXPECT_TRUE(status.is_success());
  EXPECT_EQ(200, response_code);  // HTTP OK
  EXPECT_FALSE(data.empty());

  delete fetcher_;  // Have to delete this here and not in the destructor,
                    // because the destructor won't necessarily run on the
                    // same thread that CreateFetcher() did.

  io_loop_.PostTask(FROM_HERE, new MessageLoop::QuitTask());
  // If MessageLoop::current() != io_loop_, it will be shut down when the
  // main loop returns and this thread subsequently goes out of scope.
}

void URLFetcherPostTest::CreateFetcher(const GURL& url) {
  fetcher_ = new URLFetcher(url, URLFetcher::POST, this);
  fetcher_->set_request_context(new TestURLRequestContext());
  fetcher_->set_io_loop(&io_loop_);
  fetcher_->set_upload_data("application/x-www-form-urlencoded",
                            "bobsyeruncle");
  fetcher_->Start();
}

void URLFetcherPostTest::OnURLFetchComplete(const URLFetcher* source,
                                            const GURL& url,
                                            const URLRequestStatus& status,
                                            int response_code,
                                            const ResponseCookies& cookies,
                                            const std::string& data) {
  EXPECT_EQ(std::string("bobsyeruncle"), data);
  URLFetcherTest::OnURLFetchComplete(source, url, status, response_code,
                                     cookies, data);
}

void URLFetcherHeadersTest::OnURLFetchComplete(
    const URLFetcher* source,
    const GURL& url,
    const URLRequestStatus& status,
    int response_code,
    const ResponseCookies& cookies,
    const std::string& data) {
  std::string header;
  EXPECT_TRUE(source->response_headers()->GetNormalizedHeader("cache-control",
                                                              &header));
  EXPECT_EQ("private", header);
  URLFetcherTest::OnURLFetchComplete(source, url, status, response_code,
                                     cookies, data);
}

void URLFetcherProtectTest::CreateFetcher(const GURL& url) {
  fetcher_ = new URLFetcher(url, URLFetcher::GET, this);
  fetcher_->set_request_context(new TestURLRequestContext());
  fetcher_->set_io_loop(&io_loop_);
  start_time_ = Time::Now();
  fetcher_->Start();
}

void URLFetcherProtectTest::OnURLFetchComplete(const URLFetcher* source,
                                               const GURL& url,
                                               const URLRequestStatus& status,
                                               int response_code,
                                               const ResponseCookies& cookies,
                                               const std::string& data) {
  const TimeDelta one_second = TimeDelta::FromMilliseconds(1000);
  if (response_code >= 500) {
    // Now running ServerUnavailable test.
    // It takes more than 1 second to finish all 11 requests.
    EXPECT_TRUE(Time::Now() - start_time_ >= one_second);
    EXPECT_TRUE(status.is_success());
    EXPECT_FALSE(data.empty());
    delete fetcher_;
    io_loop_.Quit();
  } else {
    // Now running Overload test.
    static int count = 0;
    count++;
    if (count < 20) {
      fetcher_->Start();
    } else {
      // We have already sent 20 requests continuously. And we expect that
      // it takes more than 1 second due to the overload pretection settings.
      EXPECT_TRUE(Time::Now() - start_time_ >= one_second);
      URLFetcherTest::OnURLFetchComplete(source, url, status, response_code,
                                         cookies, data);
    }
  }
}

URLFetcherBadHTTPSTest::URLFetcherBadHTTPSTest() {
  PathService::Get(base::DIR_SOURCE_ROOT, &cert_dir_);
  cert_dir_ = cert_dir_.Append(FILE_PATH_LITERAL("chrome"));
  cert_dir_ = cert_dir_.Append(FILE_PATH_LITERAL("test"));
  cert_dir_ = cert_dir_.Append(FILE_PATH_LITERAL("data"));
  cert_dir_ = cert_dir_.Append(FILE_PATH_LITERAL("ssl"));
  cert_dir_ = cert_dir_.Append(FILE_PATH_LITERAL("certificates"));
}

// The "server certificate expired" error should result in automatic
// cancellation of the request by
// URLRequest::Delegate::OnSSLCertificateError.
void URLFetcherBadHTTPSTest::OnURLFetchComplete(
    const URLFetcher* source,
    const GURL& url,
    const URLRequestStatus& status,
    int response_code,
    const ResponseCookies& cookies,
    const std::string& data) {
  // This part is different from URLFetcherTest::OnURLFetchComplete
  // because this test expects the request to be cancelled.
  EXPECT_EQ(URLRequestStatus::CANCELED, status.status());
  EXPECT_EQ(net::ERR_ABORTED, status.os_error());
  EXPECT_EQ(-1, response_code);
  EXPECT_TRUE(cookies.empty());
  EXPECT_TRUE(data.empty());

  // The rest is the same as URLFetcherTest::OnURLFetchComplete.
  delete fetcher_;
  io_loop_.Quit();
}

FilePath URLFetcherBadHTTPSTest::GetExpiredCertPath() {
  return cert_dir_.Append(FILE_PATH_LITERAL("expired_cert.pem"));
}

void URLFetcherCancelTest::CreateFetcher(const GURL& url) {
  fetcher_ = new URLFetcher(url, URLFetcher::GET, this);
  fetcher_->set_request_context(
      new CancelTestURLRequestContext(&context_released_));
  fetcher_->set_io_loop(&io_loop_);
  fetcher_->Start();
  // Make sure we give the IO thread a chance to run.
  timer_.Start(TimeDelta::FromMilliseconds(100), this,
               &URLFetcherCancelTest::CancelRequest);
}

void URLFetcherCancelTest::OnURLFetchComplete(const URLFetcher* source,
                                              const GURL& url,
                                              const URLRequestStatus& status,
                                              int response_code,
                                              const ResponseCookies& cookies,
                                              const std::string& data) {
  // We should have cancelled the request before completion.
  ADD_FAILURE();
  delete fetcher_;
  io_loop_.PostTask(FROM_HERE, new MessageLoop::QuitTask());
}

void URLFetcherCancelTest::CancelRequest() {
  delete fetcher_;
  timer_.Stop();
  // Make sure we give the IO thread a chance to run.
  timer_.Start(TimeDelta::FromMilliseconds(100), this,
               &URLFetcherCancelTest::TestContextReleased);
}

void URLFetcherCancelTest::TestContextReleased() {
  EXPECT_TRUE(context_released_);
  timer_.Stop();
  io_loop_.PostTask(FROM_HERE, new MessageLoop::QuitTask());
}

}  // namespace.

TEST_F(URLFetcherTest, SameThreadsTest) {
  // Create the fetcher on the main thread.  Since IO will happen on the main
  // thread, this will test URLFetcher's ability to do everything on one
  // thread.
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(kDocRoot);
  ASSERT_TRUE(NULL != server.get());

  CreateFetcher(GURL(server->TestServerPage("defaultresponse")));

  MessageLoop::current()->Run();
}

TEST_F(URLFetcherTest, DifferentThreadsTest) {
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(kDocRoot);
  ASSERT_TRUE(NULL != server.get());
  // Create a separate thread that will create the URLFetcher.  The current
  // (main) thread will do the IO, and when the fetch is complete it will
  // terminate the main thread's message loop; then the other thread's
  // message loop will be shut down automatically as the thread goes out of
  // scope.
  base::Thread t("URLFetcher test thread");
  t.Start();
  t.message_loop()->PostTask(FROM_HERE, new FetcherWrapperTask(this,
      GURL(server->TestServerPage("defaultresponse"))));

  MessageLoop::current()->Run();
}

TEST_F(URLFetcherPostTest, Basic) {
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(kDocRoot);
  ASSERT_TRUE(NULL != server.get());
  CreateFetcher(GURL(server->TestServerPage("echo")));
  MessageLoop::current()->Run();
}

TEST_F(URLFetcherHeadersTest, Headers) {
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(L"net/data/url_request_unittest");
  ASSERT_TRUE(NULL != server.get());
  CreateFetcher(GURL(server->TestServerPage("files/with-headers.html")));
  MessageLoop::current()->Run();
  // The actual tests are in the URLFetcherHeadersTest fixture.
}

TEST_F(URLFetcherProtectTest, Overload) {
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(kDocRoot);
  ASSERT_TRUE(NULL != server.get());
  GURL url = GURL(server->TestServerPage("defaultresponse"));

  // Registers an entry for test url. It only allows 3 requests to be sent
  // in 200 milliseconds.
  URLFetcherProtectManager* manager = URLFetcherProtectManager::GetInstance();
  URLFetcherProtectEntry* entry =
      new URLFetcherProtectEntry(200, 3, 11, 1, 2.0, 0, 256);
  manager->Register(url.host(), entry);

  CreateFetcher(url);

  MessageLoop::current()->Run();
}

TEST_F(URLFetcherProtectTest, ServerUnavailable) {
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(L"chrome/test/data");
  ASSERT_TRUE(NULL != server.get());
  GURL url = GURL(server->TestServerPage("files/server-unavailable.html"));

  // Registers an entry for test url. The backoff time is calculated by:
  //     new_backoff = 2.0 * old_backoff + 0
  // and maximum backoff time is 256 milliseconds.
  // Maximum retries allowed is set to 11.
  URLFetcherProtectManager* manager = URLFetcherProtectManager::GetInstance();
  URLFetcherProtectEntry* entry =
      new URLFetcherProtectEntry(200, 3, 11, 1, 2.0, 0, 256);
  manager->Register(url.host(), entry);

  CreateFetcher(url);

  MessageLoop::current()->Run();
}

#if defined(OS_WIN)
TEST_F(URLFetcherBadHTTPSTest, BadHTTPSTest) {
#else
// TODO(port): Enable BadHTTPSTest. Currently asserts in
// URLFetcherBadHTTPSTest::OnURLFetchComplete don't pass.
TEST_F(URLFetcherBadHTTPSTest, DISABLED_BadHTTPSTest) {
#endif
  scoped_refptr<HTTPSTestServer> server =
      HTTPSTestServer::CreateServer(util_.kHostName, util_.kBadHTTPSPort,
          kDocRoot, util_.GetExpiredCertPath().ToWStringHack());
  ASSERT_TRUE(NULL != server.get());

  CreateFetcher(GURL(server->TestServerPage("defaultresponse")));

  MessageLoop::current()->Run();
}

TEST_F(URLFetcherCancelTest, ReleasesContext) {
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(L"chrome/test/data");
  ASSERT_TRUE(NULL != server.get());
  GURL url = GURL(server->TestServerPage("files/server-unavailable.html"));

  // Registers an entry for test url. The backoff time is calculated by:
  //     new_backoff = 2.0 * old_backoff + 0
  // The initial backoff is 2 seconds and maximum backoff is 4 seconds.
  // Maximum retries allowed is set to 2.
  URLFetcherProtectManager* manager = URLFetcherProtectManager::GetInstance();
  URLFetcherProtectEntry* entry =
      new URLFetcherProtectEntry(200, 3, 2, 2000, 2.0, 0, 4000);
  manager->Register(url.host(), entry);

  // Create a separate thread that will create the URLFetcher.  The current
  // (main) thread will do the IO, and when the fetch is complete it will
  // terminate the main thread's message loop; then the other thread's
  // message loop will be shut down automatically as the thread goes out of
  // scope.
  base::Thread t("URLFetcher test thread");
  t.Start();
  t.message_loop()->PostTask(FROM_HERE, new FetcherWrapperTask(this, url));

  MessageLoop::current()->Run();
}
