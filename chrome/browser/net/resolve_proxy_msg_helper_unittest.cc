// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/resolve_proxy_msg_helper.h"

#include "base/waitable_event.h"
#include "net/base/net_errors.h"
#include "net/proxy/proxy_config_service.h"
#include "net/proxy/proxy_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"

// This ProxyConfigService always returns "http://pac" as the PAC url to use.
class MockProxyConfigService: public net::ProxyConfigService {
 public:
  virtual int GetProxyConfig(net::ProxyConfig* results) {
    results->pac_url = GURL("http://pac");
    return net::OK;
  }
};

// This PAC resolver always returns the hostname of the query URL as the
// proxy to use. The Block() method will make GetProxyForURL() hang until
// Unblock() is called.
class MockProxyResolver : public net::ProxyResolver {
 public:
  MockProxyResolver() : ProxyResolver(true), event_(false, false),
                        is_blocked_(false) {
  }

  virtual int GetProxyForURL(const GURL& query_url,
                             const GURL& /*pac_url*/,
                             net::ProxyInfo* results) {
    if (is_blocked_)
      event_.Wait();
    results->UseNamedProxy(query_url.host());
    return net::OK;
  }

  void Block() {
    is_blocked_ = true;
    event_.Reset();
  }

  void Unblock() {
    is_blocked_ = false;
    event_.Signal();
  }

 private:
  base::WaitableEvent event_;
  bool is_blocked_;
};

// This struct holds the values that were passed to
// Delegate::OnResolveProxyCompleted(). The caller should use WaitUntilDone()
// to block until the result has been populated.
struct ResultFuture {
 public:
  ResultFuture()
      : reply_msg(NULL),
        error_code(0),
        started_(false, false),
        completed_(false, false) {
  }

  // Wait until the request has completed. In other words we have invoked:
  // ResolveProxyMsgHelper::Delegate::OnResolveProxyCompleted.
  void WaitUntilDone() {
    completed_.Wait();
  }

  // Wait until the request has been sent to ResolveProxyMsgHelper.
  void WaitUntilStarted() {
    started_.Wait();
  }

  bool TimedWaitUntilDone(const base::TimeDelta& max_time) {
    return completed_.TimedWait(max_time);
  }

  // These fields are only valid after returning from WaitUntilDone().
  IPC::Message* reply_msg;
  int error_code;
  std::string proxy_list;

 private:
  friend class AsyncRequestRunner;
  base::WaitableEvent started_;
  base::WaitableEvent completed_;
};

// This class lives on the io thread. It starts async requests using the
// class under test (ResolveProxyMsgHelper), and signals the result future on
// completion.
class AsyncRequestRunner : public ResolveProxyMsgHelper::Delegate {
 public:
  AsyncRequestRunner(net::ProxyService* proxy_service) {
    resolve_proxy_msg_helper_.reset(
        new ResolveProxyMsgHelper(this, proxy_service));
  }

  void Start(ResultFuture* future, const GURL& url, IPC::Message* reply_msg) {
    futures_.push_back(future);
    resolve_proxy_msg_helper_->Start(url, reply_msg);

    // Notify of request start.
    future->started_.Signal();
  }

  virtual void OnResolveProxyCompleted(IPC::Message* reply_msg,
                                       int error_code,
                                       const std::string& proxy_list) {
    // Update the result future for this request (top of queue), and signal it.
    ResultFuture* future = futures_.front();
    futures_.pop_front();

    future->reply_msg = reply_msg;
    future->error_code = error_code;
    future->proxy_list = proxy_list;

    // Notify of request completion.
    future->completed_.Signal();
  }

 private:
  std::deque<ResultFuture*> futures_;
  scoped_ptr<ResolveProxyMsgHelper> resolve_proxy_msg_helper_;
};

// Helper class to start async requests on an io thread, and return a
// result future. The caller then uses ResultFuture::WaitUntilDone() to
// get at the results. It "bridges" the originating thread with the helper
// io thread.
class RunnerBridge {
 public:
  RunnerBridge() : io_thread_("io_thread"), done_(true, false) {
    // Start an io thread where we will run the async requests.
    base::Thread::Options options;
    options.message_loop_type = MessageLoop::TYPE_IO;
    io_thread_.StartWithOptions(options);

    // Construct the state that lives on io thread.
    io_thread_.message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
        this, &RunnerBridge::DoConstruct));
    done_.Wait();
  }

  // Start an async request on the io thread.
  ResultFuture* Start(const GURL& url, IPC::Message* reply_msg) {
    ResultFuture* future = new ResultFuture();

    io_thread_.message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
        async_runner_, &AsyncRequestRunner::Start, future, url, reply_msg));

    return future;
  }

  void DestroyAsyncRunner() {
    io_thread_.message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
        this, &RunnerBridge::DoDestroyAsyncRunner));
    done_.Wait();
  }

  ~RunnerBridge() {
    io_thread_.message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
        this, &RunnerBridge::DoDestroy));
    done_.Wait();
  }

  MockProxyResolver* proxy_resolver() {
    return proxy_resolver_;
  }

  // Called from io thread.
  void DoConstruct() {
    proxy_resolver_ = new MockProxyResolver();
    proxy_service_ = new net::ProxyService(new MockProxyConfigService(),
                                           proxy_resolver_);
    async_runner_ = new AsyncRequestRunner(proxy_service_);
    done_.Signal();
  }

  // Called from io thread.
  void DoDestroy() {
    delete async_runner_;
    delete proxy_service_;
    done_.Signal();
  }

  // Called from io thread.
  void DoDestroyAsyncRunner() {
    delete async_runner_;
    async_runner_ = NULL;
    done_.Signal();
  }

 private:
  base::Thread io_thread_;
  base::WaitableEvent done_;

  net::ProxyService* proxy_service_;
  MockProxyResolver* proxy_resolver_;  // Owned by proxy_service_.

  AsyncRequestRunner* async_runner_;
};

// Avoid the need to have an AddRef / Release
template<>
void RunnableMethodTraits<RunnerBridge>::RetainCallee(RunnerBridge*) {}
template<>
void RunnableMethodTraits<RunnerBridge>::ReleaseCallee(RunnerBridge*) {}

template<>
void RunnableMethodTraits<AsyncRequestRunner>::RetainCallee(AsyncRequestRunner*) {}
template<>
void RunnableMethodTraits<AsyncRequestRunner>::ReleaseCallee(AsyncRequestRunner*) {}


// Issue three sequential requests -- each should succeed.
TEST(ResolveProxyMsgHelperTest, Sequential) {
  RunnerBridge runner;

  GURL url1("http://www.google1.com/");
  GURL url2("http://www.google2.com/");
  GURL url3("http://www.google3.com/");

  scoped_ptr<IPC::Message> msg1(new IPC::Message());
  scoped_ptr<IPC::Message> msg2(new IPC::Message());
  scoped_ptr<IPC::Message> msg3(new IPC::Message());

  // Execute each request sequentially (so there are never 2 requests
  // outstanding at the same time).

  scoped_ptr<ResultFuture> result1(runner.Start(url1, msg1.get()));
  result1->WaitUntilDone();

  scoped_ptr<ResultFuture> result2(runner.Start(url2, msg2.get()));
  result2->WaitUntilDone();

  scoped_ptr<ResultFuture> result3(runner.Start(url3, msg3.get()));
  result3->WaitUntilDone();

  // Check that each request gave the expected result.

  EXPECT_EQ(msg1.get(), result1->reply_msg);
  EXPECT_EQ(net::OK, result1->error_code);
  EXPECT_EQ("PROXY www.google1.com:80", result1->proxy_list);

  EXPECT_EQ(msg2.get(), result2->reply_msg);
  EXPECT_EQ(net::OK, result2->error_code);
  EXPECT_EQ("PROXY www.google2.com:80", result2->proxy_list);

  EXPECT_EQ(msg3.get(), result3->reply_msg);
  EXPECT_EQ(net::OK, result3->error_code);
  EXPECT_EQ("PROXY www.google3.com:80", result3->proxy_list);
}

// Issue a request while one is already in progress -- should be queued.
TEST(ResolveProxyMsgHelperTest, QueueRequests) {
  RunnerBridge runner;

  GURL url1("http://www.google1.com/");
  GURL url2("http://www.google2.com/");
  GURL url3("http://www.google3.com/");

  scoped_ptr<IPC::Message> msg1(new IPC::Message());
  scoped_ptr<IPC::Message> msg2(new IPC::Message());
  scoped_ptr<IPC::Message> msg3(new IPC::Message());

  // Make the proxy resolver hang on the next request.
  runner.proxy_resolver()->Block();

  // Start three requests. Since the proxy resolver is hung, the second two
  // will be pending.

  scoped_ptr<ResultFuture> result1(runner.Start(url1, msg1.get()));
  scoped_ptr<ResultFuture> result2(runner.Start(url2, msg2.get()));
  scoped_ptr<ResultFuture> result3(runner.Start(url3, msg3.get()));

  // Wait for the final request to have been scheduled. Otherwise we may rush
  // to calling Unblock() without actually having blocked anything.
  result3->WaitUntilStarted();

  // Unblock the proxy service so requests 1-3 can complete.
  runner.proxy_resolver()->Unblock();

  // Wait for all the requests to finish (they run in FIFO order).
  result3->WaitUntilDone();

  // Check that each call invoked the callback with the right parameters.

  EXPECT_EQ(msg1.get(), result1->reply_msg);
  EXPECT_EQ(net::OK, result1->error_code);
  EXPECT_EQ("PROXY www.google1.com:80", result1->proxy_list);

  EXPECT_EQ(msg2.get(), result2->reply_msg);
  EXPECT_EQ(net::OK, result2->error_code);
  EXPECT_EQ("PROXY www.google2.com:80", result2->proxy_list);

  EXPECT_EQ(msg3.get(), result3->reply_msg);
  EXPECT_EQ(net::OK, result3->error_code);
  EXPECT_EQ("PROXY www.google3.com:80", result3->proxy_list);
}

// Delete the helper while a request is in progress, and others are pending.
TEST(ResolveProxyMsgHelperTest, CancelPendingRequests) {
  RunnerBridge runner;

  GURL url1("http://www.google1.com/");
  GURL url2("http://www.google2.com/");
  GURL url3("http://www.google3.com/");

  // NOTE: these are not scoped ptr, since they will be deleted by the
  // request's cancellation.
  IPC::Message* msg1 = new IPC::Message();
  IPC::Message* msg2 = new IPC::Message();
  IPC::Message* msg3 = new IPC::Message();

  // Make the next request block.
  runner.proxy_resolver()->Block();

  // Start three requests; since the first one blocked, the other two should
  // be pending.

  scoped_ptr<ResultFuture> result1(runner.Start(url1, msg1));
  scoped_ptr<ResultFuture> result2(runner.Start(url2, msg2));
  scoped_ptr<ResultFuture> result3(runner.Start(url3, msg3));

  result3->WaitUntilStarted();

  // Delete the underlying ResolveProxyMsgHelper -- this should cancel all
  // the requests which are outstanding.
  runner.DestroyAsyncRunner();

  // Unblocking the proxy resolver means the three requests can complete --
  // however they should not try to notify the delegate since we have already
  // deleted the helper.
  runner.proxy_resolver()->Unblock();

  // Check that none of the requests were sent to the delegate.
  EXPECT_FALSE(
      result1->TimedWaitUntilDone(base::TimeDelta::FromMilliseconds(2)));
  EXPECT_FALSE(
      result2->TimedWaitUntilDone(base::TimeDelta::FromMilliseconds(2)));
  EXPECT_FALSE(
      result3->TimedWaitUntilDone(base::TimeDelta::FromMilliseconds(2)));

  // It should also be the case that msg1, msg2, msg3 were deleted by the
  // cancellation. (Else will show up as a leak in Purify).
}

