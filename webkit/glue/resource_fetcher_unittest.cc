// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "base/compiler_specific.h"

MSVC_PUSH_WARNING_LEVEL(0);
#include "ResourceResponse.h"
MSVC_POP_WARNING();
#undef LOG

#include "webkit/glue/unittest_test_server.h"
#include "webkit/glue/webview.h"
#include "webkit/glue/webframe_impl.h"
#include "webkit/glue/resource_fetcher.h"
#include "webkit/tools/test_shell/simple_resource_loader_bridge.h"
#include "webkit/tools/test_shell/test_shell_test.h"

using WebCore::ResourceResponse;

namespace {


class ResourceFetcherTests : public TestShellTest {
 public:
  void SetUp() {
    TestShellTest::SetUp();
  }
  void TearDown() {
    TestShellTest::TearDown();
  }
};

static const int kMaxWaitTimeMs = 5000;
static const int kWaitIntervalMs = 100;

class FetcherDelegate : public ResourceFetcher::Delegate {
 public:
  FetcherDelegate()
      : timer_id_(0), completed_(false), time_elapsed_ms_(0) {
    // Start a repeating timer waiting for the download to complete.  The
    // callback has to be a static function, so we hold on to our instance.
    FetcherDelegate::instance_ = this;
    timer_id_ = SetTimer(NULL, NULL, kWaitIntervalMs,
                         &FetcherDelegate::TimerCallback);
  }
  
  virtual void OnURLFetchComplete(const ResourceResponse& response,
                                  const std::string& data) {
    response_ = response;
    data_ = data;
    completed_ = true;
    KillTimer(NULL, timer_id_);
    MessageLoop::current()->Quit();
  }

  bool completed() const { return completed_; }
  bool timed_out() const { return time_elapsed_ms_ > kMaxWaitTimeMs; }

  int time_elapsed_ms() const { return time_elapsed_ms_; }
  std::string data() const { return data_; }
  ResourceResponse response() const { return response_; }

  // Wait for the request to complete or timeout.  We use a loop here b/c the
  // testing infrastructure (test_shell) can generate spurious calls to the
  // MessageLoop's Quit method.
  void WaitForResponse() {
    while (!completed() && !timed_out())
      MessageLoop::current()->Run();
  }

  // Static timer callback, just passes through to instance version.
  static VOID CALLBACK TimerCallback(HWND hwnd, UINT msg, UINT_PTR timer_id,
                                     DWORD ms) {
    instance_->TimerFired(hwnd, timer_id);
  }
  
  void TimerFired(HWND hwnd, UINT_PTR timer_id) {
    ASSERT_FALSE(completed_);

    if (timed_out()) {
      printf("timer fired\n");
      KillTimer(hwnd, timer_id);
      MessageLoop::current()->Quit();
      FAIL() << "fetch timed out";
      return;
    }

    time_elapsed_ms_ += kWaitIntervalMs;
  }

  static FetcherDelegate* instance_;

 private:
  UINT_PTR timer_id_;
  bool completed_;
  int time_elapsed_ms_;
  ResourceResponse response_;
  std::string data_;
};

FetcherDelegate* FetcherDelegate::instance_ = NULL;

}  // namespace

// Test a fetch from the test server.
TEST_F(ResourceFetcherTests, ResourceFetcherDownload) {
  UnittestTestServer server;

  WebFrame* web_frame = test_shell_->webView()->GetMainFrame();
  // Not safe, but this is a unittest, so whatever.
  WebFrameImpl* web_frame_impl = reinterpret_cast<WebFrameImpl*>(web_frame);
  WebCore::Frame* frame = web_frame_impl->frame();

  GURL url = server.TestServerPage("files/test_shell/index.html");
  scoped_ptr<FetcherDelegate> delegate(new FetcherDelegate);
  scoped_ptr<ResourceFetcher> fetcher(new ResourceFetcher(
      url, frame, delegate.get()));

  delegate->WaitForResponse();

  ASSERT_TRUE(delegate->completed());
  EXPECT_EQ(delegate->response().httpStatusCode(), 200);
  std::string text = delegate->data();
  EXPECT_TRUE(text.find("What is this page?") != std::string::npos);

  // Test 404 response.
  url = server.TestServerPage("files/thisfiledoesntexist.html");
  delegate.reset(new FetcherDelegate);
  fetcher.reset(new ResourceFetcher(url, frame, delegate.get()));

  delegate->WaitForResponse();

  ASSERT_TRUE(delegate->completed());
  EXPECT_EQ(delegate->response().httpStatusCode(), 404);
  EXPECT_TRUE(delegate->data().find("Not Found.") != std::string::npos);
}

TEST_F(ResourceFetcherTests, ResourceFetcherDidFail) {
  UnittestTestServer server;
  WebFrame* web_frame = test_shell_->webView()->GetMainFrame();
  // Not safe, but this is a unittest, so whatever.
  WebFrameImpl* web_frame_impl = reinterpret_cast<WebFrameImpl*>(web_frame);
  WebCore::Frame* frame = web_frame_impl->frame();

  // Try to fetch a page on a site that doesn't exist.
  GURL url("http://localhost:1339/doesnotexist");
  scoped_ptr<FetcherDelegate> delegate(new FetcherDelegate);
  scoped_ptr<ResourceFetcher> fetcher(new ResourceFetcher(
      url, frame, delegate.get()));

  delegate->WaitForResponse();

  // When we fail, we still call the Delegate callback but we pass in empty
  // values.
  EXPECT_TRUE(delegate->completed());
  EXPECT_TRUE(delegate->response().isNull());
  EXPECT_EQ(delegate->data(), std::string());
  EXPECT_TRUE(delegate->time_elapsed_ms() < kMaxWaitTimeMs);
}

TEST_F(ResourceFetcherTests, ResourceFetcherTimeout) {
  UnittestTestServer server;

  WebFrame* web_frame = test_shell_->webView()->GetMainFrame();
  // Not safe, but this is a unittest, so whatever.
  WebFrameImpl* web_frame_impl = reinterpret_cast<WebFrameImpl*>(web_frame);
  WebCore::Frame* frame = web_frame_impl->frame();

  // Grab a page that takes at least 1 sec to respond, but set the fetcher to
  // timeout in 0 sec.
  GURL url = server.TestServerPage("slow?1");
  scoped_ptr<FetcherDelegate> delegate(new FetcherDelegate);
  scoped_ptr<ResourceFetcher> fetcher(new ResourceFetcherWithTimeout(
      url, frame, 0, delegate.get()));

  delegate->WaitForResponse();

  // When we timeout, we still call the Delegate callback but we pass in empty
  // values.
  EXPECT_TRUE(delegate->completed());
  EXPECT_TRUE(delegate->response().isNull());
  EXPECT_EQ(delegate->data(), std::string());
  EXPECT_TRUE(delegate->time_elapsed_ms() < kMaxWaitTimeMs);
}

