// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/tcp_client_socket_pool.h"

#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "net/base/host_resolver_unittest.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/socket/client_socket.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/client_socket_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

const int kMaxSocketsPerGroup = 6;

// Note that the first and the last are the same, the first should be handled
// before the last, since it was inserted first.
const int kPriorities[10] = { 1, 7, 9, 5, 6, 2, 8, 3, 4, 1 };

// This is the number of extra requests beyond the first few that use up all
// available sockets in the socket group.
const int kNumPendingRequests = arraysize(kPriorities);

const int kNumRequests = kMaxSocketsPerGroup + kNumPendingRequests;

class MockClientSocket : public ClientSocket {
 public:
  MockClientSocket() : connected_(false) {}

  // ClientSocket methods:
  virtual int Connect(CompletionCallback* callback) {
    connected_ = true;
    return OK;
  }
  virtual void Disconnect() {
    connected_ = false;
  }
  virtual bool IsConnected() const {
    return connected_;
  }
  virtual bool IsConnectedAndIdle() const {
    return connected_;
  }

  // Socket methods:
  virtual int Read(IOBuffer* buf, int buf_len,
                   CompletionCallback* callback) {
    return ERR_FAILED;
  }
  virtual int Write(IOBuffer* buf, int buf_len,
                    CompletionCallback* callback) {
    return ERR_FAILED;
  }

 private:
  bool connected_;
};

class MockFailingClientSocket : public ClientSocket {
 public:
  MockFailingClientSocket() {}

  // ClientSocket methods:
  virtual int Connect(CompletionCallback* callback) {
    return ERR_CONNECTION_FAILED;
  }

  virtual void Disconnect() {}

  virtual bool IsConnected() const {
    return false;
  }
  virtual bool IsConnectedAndIdle() const {
    return false;
  }

  // Socket methods:
  virtual int Read(IOBuffer* buf, int buf_len,
                   CompletionCallback* callback) {
    return ERR_FAILED;
  }

  virtual int Write(IOBuffer* buf, int buf_len,
                    CompletionCallback* callback) {
    return ERR_FAILED;
  }
};

class MockPendingClientSocket : public ClientSocket {
 public:
  MockPendingClientSocket(bool should_connect)
      : method_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
        should_connect_(should_connect),
        is_connected_(false) {}

  // ClientSocket methods:
  virtual int Connect(CompletionCallback* callback) {
    MessageLoop::current()->PostTask(
        FROM_HERE,
        method_factory_.NewRunnableMethod(
           &MockPendingClientSocket::DoCallback, callback));
    return ERR_IO_PENDING;
  }

  virtual void Disconnect() {}

  virtual bool IsConnected() const {
    return is_connected_;
  }
  virtual bool IsConnectedAndIdle() const {
    return is_connected_;
  }

  // Socket methods:
  virtual int Read(IOBuffer* buf, int buf_len,
                   CompletionCallback* callback) {
    return ERR_FAILED;
  }

  virtual int Write(IOBuffer* buf, int buf_len,
                    CompletionCallback* callback) {
    return ERR_FAILED;
  }

 private:
  void DoCallback(CompletionCallback* callback) {
    if (should_connect_) {
      is_connected_ = true;
      callback->Run(OK);
    } else {
      is_connected_ = false;
      callback->Run(ERR_CONNECTION_FAILED);
    }
  }

  ScopedRunnableMethodFactory<MockPendingClientSocket> method_factory_;
  bool should_connect_;
  bool is_connected_;
};

class MockClientSocketFactory : public ClientSocketFactory {
 public:
  enum ClientSocketType {
    MOCK_CLIENT_SOCKET,
    MOCK_FAILING_CLIENT_SOCKET,
    MOCK_PENDING_CLIENT_SOCKET,
    MOCK_PENDING_FAILING_CLIENT_SOCKET,
  };

  MockClientSocketFactory()
      : allocation_count_(0), client_socket_type_(MOCK_CLIENT_SOCKET) {}

  virtual ClientSocket* CreateTCPClientSocket(const AddressList& addresses) {
    allocation_count_++;
    switch (client_socket_type_) {
      case MOCK_CLIENT_SOCKET:
        return new MockClientSocket();
      case MOCK_FAILING_CLIENT_SOCKET:
        return new MockFailingClientSocket();
      case MOCK_PENDING_CLIENT_SOCKET:
        return new MockPendingClientSocket(true);
      case MOCK_PENDING_FAILING_CLIENT_SOCKET:
        return new MockPendingClientSocket(false);
      default:
        NOTREACHED();
        return new MockClientSocket();
    }
  }

  virtual SSLClientSocket* CreateSSLClientSocket(
      ClientSocket* transport_socket,
      const std::string& hostname,
      const SSLConfig& ssl_config) {
    NOTIMPLEMENTED();
    return NULL;
  }

  int allocation_count() const { return allocation_count_; }

  void set_client_socket_type(ClientSocketType type) {
    client_socket_type_ = type;
  }

 private:
  int allocation_count_;
  ClientSocketType client_socket_type_;
};

class TestSocketRequest : public CallbackRunner< Tuple1<int> > {
 public:
  TestSocketRequest(
      ClientSocketPool* pool,
      std::vector<TestSocketRequest*>* request_order)
      : handle(pool), request_order_(request_order) {}

  ClientSocketHandle handle;

  int WaitForResult() {
    return callback_.WaitForResult();
  }

  virtual void RunWithParams(const Tuple1<int>& params) {
    callback_.RunWithParams(params);
    completion_count++;
    request_order_->push_back(this);
  }

  static int completion_count;

 private:
  std::vector<TestSocketRequest*>* request_order_;
  TestCompletionCallback callback_;
};

int TestSocketRequest::completion_count = 0;

class TCPClientSocketPoolTest : public testing::Test {
 protected:
  TCPClientSocketPoolTest()
      : pool_(new TCPClientSocketPool(kMaxSocketsPerGroup,
                                      new HostResolver,
                                      &client_socket_factory_)) {}

  virtual void SetUp() {
    RuleBasedHostMapper *host_mapper = new RuleBasedHostMapper();
    host_mapper->AddRule("*", "127.0.0.1");
    scoped_host_mapper_.Init(host_mapper);
    TestSocketRequest::completion_count = 0;
  }

  virtual void TearDown() {
    // The tests often call Reset() on handles at the end which may post
    // DoReleaseSocket() tasks.
    MessageLoop::current()->RunAllPending();
  }

  ScopedHostMapper scoped_host_mapper_;
  MockClientSocketFactory client_socket_factory_;
  scoped_refptr<ClientSocketPool> pool_;
  std::vector<TestSocketRequest*> request_order_;
};

TEST_F(TCPClientSocketPoolTest, Basic) {
  TestCompletionCallback callback;
  ClientSocketHandle handle(pool_.get());
  HostResolver::RequestInfo info("www.google.com", 80);
  int rv = handle.Init("a", info, 0, &callback);
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  EXPECT_EQ(OK, callback.WaitForResult());
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());

  handle.Reset();
}

TEST_F(TCPClientSocketPoolTest, InitHostResolutionFailure) {
  RuleBasedHostMapper* host_mapper = new RuleBasedHostMapper;
  host_mapper->AddSimulatedFailure("unresolvable.host.name");
  ScopedHostMapper scoped_host_mapper(host_mapper);
  TestSocketRequest req(pool_.get(), &request_order_);
  HostResolver::RequestInfo info("unresolvable.host.name", 80);
  EXPECT_EQ(ERR_IO_PENDING, req.handle.Init("a", info, 5, &req));
  EXPECT_EQ(ERR_NAME_NOT_RESOLVED, req.WaitForResult());
}

TEST_F(TCPClientSocketPoolTest, InitConnectionFailure) {
  client_socket_factory_.set_client_socket_type(
      MockClientSocketFactory::MOCK_FAILING_CLIENT_SOCKET);
  TestSocketRequest req(pool_.get(), &request_order_);
  HostResolver::RequestInfo info("a", 80);
  EXPECT_EQ(ERR_IO_PENDING,
            req.handle.Init("a", info, 5, &req));
  EXPECT_EQ(ERR_CONNECTION_FAILED, req.WaitForResult());
  // HostCache caches it, so MockFailingClientSocket will cause Init() to
  // synchronously fail.
  EXPECT_EQ(ERR_CONNECTION_FAILED,
            req.handle.Init("a", info, 5, &req));
}

TEST_F(TCPClientSocketPoolTest, PendingRequests) {
  scoped_ptr<TestSocketRequest> reqs[kNumRequests];

  for (size_t i = 0; i < arraysize(reqs); ++i)
    reqs[i].reset(new TestSocketRequest(pool_.get(), &request_order_));

  // Create connections or queue up requests.

  // First request finishes asynchronously.
  HostResolver::RequestInfo info("www.google.com", 80);
  int rv = reqs[0]->handle.Init("a", info, 5, reqs[0].get());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(OK, reqs[0]->WaitForResult());

  // Rest of them finish synchronously, since they're in the HostCache.
  for (int i = 1; i < kMaxSocketsPerGroup; ++i) {
    rv = reqs[i]->handle.Init("a", info, 5, reqs[i].get());
    EXPECT_EQ(OK, rv);
    request_order_.push_back(reqs[i].get());
  }

  // The rest are pending since we've used all active sockets.
  for (int i = 0; i < kNumPendingRequests; ++i) {
    rv = reqs[kMaxSocketsPerGroup + i]->handle.Init(
        "a", info, kPriorities[i], reqs[kMaxSocketsPerGroup + i].get());
    EXPECT_EQ(ERR_IO_PENDING, rv);
  }

  // Release any connections until we have no connections.
  bool released_one;
  do {
    released_one = false;
    for (size_t i = 0; i < arraysize(reqs); ++i) {
      if (reqs[i]->handle.is_initialized()) {
        reqs[i]->handle.Reset();
        MessageLoop::current()->RunAllPending();
        released_one = true;
      }
    }
  } while (released_one);

  EXPECT_EQ(kMaxSocketsPerGroup, client_socket_factory_.allocation_count());
  EXPECT_EQ(kNumPendingRequests + 1, TestSocketRequest::completion_count);

  for (int i = 0; i < kMaxSocketsPerGroup; ++i) {
    EXPECT_EQ(request_order_[i], reqs[i].get()) <<
        "Request " << i << " was not in order.";
  }

  for (int i = 0; i < kNumPendingRequests - 1; ++i) {
    int index_in_queue = (kNumPendingRequests - 1) - kPriorities[i];
    EXPECT_EQ(request_order_[kMaxSocketsPerGroup + index_in_queue],
              reqs[kMaxSocketsPerGroup + i].get()) <<
        "Request " << kMaxSocketsPerGroup + i << " was not in order.";
  }

  EXPECT_EQ(request_order_[arraysize(reqs) - 1],
            reqs[arraysize(reqs) - 1].get()) <<
      "The last request with priority 1 should not have been inserted "
      "earlier into the queue.";
}

TEST_F(TCPClientSocketPoolTest, PendingRequests_NoKeepAlive) {
  scoped_ptr<TestSocketRequest> reqs[kNumRequests];
  for (size_t i = 0; i < arraysize(reqs); ++i)
    reqs[i].reset(new TestSocketRequest(pool_.get(), &request_order_));

  // Create connections or queue up requests.

  // First request finishes asynchronously.
  HostResolver::RequestInfo info("www.google.com", 80);
  int rv = reqs[0]->handle.Init("a", info, 5, reqs[0].get());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(OK, reqs[0]->WaitForResult());

  // Rest of them finish synchronously, since they're in the HostCache.
  for (int i = 1; i < kMaxSocketsPerGroup; ++i) {
    rv = reqs[i]->handle.Init("a", info, 5, reqs[i].get());
    EXPECT_EQ(OK, rv);
    request_order_.push_back(reqs[i].get());
  }

  // The rest are pending since we've used all active sockets.
  for (int i = 0; i < kNumPendingRequests; ++i) {
    EXPECT_EQ(ERR_IO_PENDING, reqs[kMaxSocketsPerGroup + i]->handle.Init(
        "a", info, 0, reqs[kMaxSocketsPerGroup + i].get()));
  }

  // Release any connections until we have no connections.
  bool released_one;
  do {
    released_one = false;
    for (size_t i = 0; i < arraysize(reqs); ++i) {
      if (reqs[i]->handle.is_initialized()) {
        reqs[i]->handle.socket()->Disconnect();  // No keep alive.
        reqs[i]->handle.Reset();
        MessageLoop::current()->RunAllPending();
        released_one = true;
      }
    }
  } while (released_one);

  for (int i = kMaxSocketsPerGroup; i < kNumRequests; ++i)
    EXPECT_EQ(OK, reqs[i]->WaitForResult());

  EXPECT_EQ(kNumRequests, client_socket_factory_.allocation_count());
  EXPECT_EQ(kNumPendingRequests + 1, TestSocketRequest::completion_count);
}

// This test will start up a RequestSocket() and then immediately Cancel() it.
// The pending host resolution will eventually complete, and destroy the
// ClientSocketPool which will crash if the group was not cleared properly.
TEST_F(TCPClientSocketPoolTest, CancelRequestClearGroup) {
  TestSocketRequest req(pool_.get(), &request_order_);
  HostResolver::RequestInfo info("www.google.com", 80);
  EXPECT_EQ(ERR_IO_PENDING, req.handle.Init("a", info, 5, &req));
  req.handle.Reset();

  PlatformThread::Sleep(100);

  // There is a race condition here.  If the worker pool doesn't post the task
  // before we get here, then this might not run ConnectingSocket::OnIOComplete
  // and therefore leak the canceled ConnectingSocket.  However, other tests
  // after this will call MessageLoop::RunAllPending() which should prevent a
  // leak, unless the worker thread takes longer than all of them.
  MessageLoop::current()->RunAllPending();
}

TEST_F(TCPClientSocketPoolTest, TwoRequestsCancelOne) {
  TestSocketRequest req(pool_.get(), &request_order_);
  TestSocketRequest req2(pool_.get(), &request_order_);

  HostResolver::RequestInfo info("www.google.com", 80);
  EXPECT_EQ(ERR_IO_PENDING, req.handle.Init("a", info, 5, &req));
  EXPECT_EQ(ERR_IO_PENDING, req2.handle.Init("a", info, 5, &req2));

  req.handle.Reset();

  EXPECT_EQ(OK, req2.WaitForResult());
  req2.handle.Reset();
}

TEST_F(TCPClientSocketPoolTest, ConnectCancelConnect) {
  client_socket_factory_.set_client_socket_type(
      MockClientSocketFactory::MOCK_PENDING_CLIENT_SOCKET);
  ClientSocketHandle handle(pool_.get());
  TestCompletionCallback callback;
  TestSocketRequest req(pool_.get(), &request_order_);

  HostResolver::RequestInfo info("www.google.com", 80);
  EXPECT_EQ(ERR_IO_PENDING, handle.Init("a", info, 5, &callback));

  handle.Reset();

  TestCompletionCallback callback2;
  EXPECT_EQ(ERR_IO_PENDING, handle.Init("a", info, 5, &callback2));

  // At this point, handle has two ConnectingSockets out for it.  Due to the
  // host cache, the host resolution for both will return in the same loop of
  // the MessageLoop.  The client socket is a pending socket, so the Connect()
  // will asynchronously complete on the next loop of the MessageLoop.  That
  // means that the first ConnectingSocket will enter OnIOComplete, and then the
  // second one will.  If the first one is not cancelled, it will advance the
  // load state, and then the second one will crash.

  EXPECT_EQ(OK, callback2.WaitForResult());
  EXPECT_FALSE(callback.have_result());

  handle.Reset();
}

TEST_F(TCPClientSocketPoolTest, CancelRequest) {
  scoped_ptr<TestSocketRequest> reqs[kNumRequests];

  for (size_t i = 0; i < arraysize(reqs); ++i)
    reqs[i].reset(new TestSocketRequest(pool_.get(), &request_order_));

  // Create connections or queue up requests.
  HostResolver::RequestInfo info("www.google.com", 80);

  // First request finishes asynchronously.
  int rv = reqs[0]->handle.Init("a", info, 5, reqs[0].get());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(OK, reqs[0]->WaitForResult());

  // Rest of them finish synchronously, since they're in the HostCache.
  for (int i = 1; i < kMaxSocketsPerGroup; ++i) {
    rv = reqs[i]->handle.Init("a", info, 5, reqs[i].get());
    EXPECT_EQ(OK, rv);
    request_order_.push_back(reqs[i].get());
  }

  // The rest are pending since we've used all active sockets.
  for (int i = 0; i < kNumPendingRequests; ++i) {
    EXPECT_EQ(ERR_IO_PENDING, reqs[kMaxSocketsPerGroup + i]->handle.Init(
        "a", info, kPriorities[i], reqs[kMaxSocketsPerGroup + i].get()));
  }

  // Cancel a request.
  size_t index_to_cancel = kMaxSocketsPerGroup + 2;
  EXPECT_TRUE(!reqs[index_to_cancel]->handle.is_initialized());
  reqs[index_to_cancel]->handle.Reset();

  // Release any connections until we have no connections.
  bool released_one;
  do {
    released_one = false;
    for (size_t i = 0; i < arraysize(reqs); ++i) {
      if (reqs[i]->handle.is_initialized()) {
        reqs[i]->handle.Reset();
        MessageLoop::current()->RunAllPending();
        released_one = true;
      }
    }
  } while (released_one);

  EXPECT_EQ(kMaxSocketsPerGroup, client_socket_factory_.allocation_count());
  EXPECT_EQ(kNumPendingRequests, TestSocketRequest::completion_count);

  for (int i = 0; i < kMaxSocketsPerGroup; ++i) {
    EXPECT_EQ(request_order_[i], reqs[i].get()) <<
        "Request " << i << " was not in order.";
  }

  for (int i = 0; i < kNumPendingRequests - 1; ++i) {
    if (i == 2) continue;
    int index_in_queue = (kNumPendingRequests - 1) - kPriorities[i];
    if (kPriorities[i] < kPriorities[index_to_cancel - kMaxSocketsPerGroup])
      index_in_queue--;
    EXPECT_EQ(request_order_[kMaxSocketsPerGroup + index_in_queue],
              reqs[kMaxSocketsPerGroup + i].get()) <<
        "Request " << kMaxSocketsPerGroup + i << " was not in order.";
  }

  EXPECT_EQ(request_order_[arraysize(reqs) - 2],
            reqs[arraysize(reqs) - 1].get()) <<
      "The last request with priority 1 should not have been inserted "
      "earlier into the queue.";
}

class RequestSocketCallback : public CallbackRunner< Tuple1<int> > {
 public:
  RequestSocketCallback(ClientSocketHandle* handle)
      : handle_(handle),
        within_callback_(false) {}

  virtual void RunWithParams(const Tuple1<int>& params) {
    callback_.RunWithParams(params);
    ASSERT_EQ(OK, params.a);

    if (!within_callback_) {
      handle_->Reset();
      within_callback_ = true;
      int rv = handle_->Init(
          "a", HostResolver::RequestInfo("www.google.com", 80), 0, this);
      EXPECT_EQ(OK, rv);
    }
  }

  int WaitForResult() {
    return callback_.WaitForResult();
  }

 private:
  ClientSocketHandle* const handle_;
  bool within_callback_;
  TestCompletionCallback callback_;
};

TEST_F(TCPClientSocketPoolTest, RequestTwice) {
  ClientSocketHandle handle(pool_.get());
  RequestSocketCallback callback(&handle);
  int rv = handle.Init(
      "a", HostResolver::RequestInfo("www.google.com", 80), 0, &callback);
  ASSERT_EQ(ERR_IO_PENDING, rv);

  EXPECT_EQ(OK, callback.WaitForResult());

  handle.Reset();
}

// Make sure that pending requests get serviced after active requests get
// cancelled.
TEST_F(TCPClientSocketPoolTest, CancelActiveRequestWithPendingRequests) {
  client_socket_factory_.set_client_socket_type(
      MockClientSocketFactory::MOCK_PENDING_CLIENT_SOCKET);

  scoped_ptr<TestSocketRequest> reqs[kNumRequests];

  // Queue up all the requests

  HostResolver::RequestInfo info("www.google.com", 80);
  for (size_t i = 0; i < arraysize(reqs); ++i) {
    reqs[i].reset(new TestSocketRequest(pool_.get(), &request_order_));
    int rv = reqs[i]->handle.Init("a", info, 5, reqs[i].get());
    EXPECT_EQ(ERR_IO_PENDING, rv);
  }

  // Now, kMaxSocketsPerGroup requests should be active.  Let's cancel them.
  for (int i = 0; i < kMaxSocketsPerGroup; ++i)
    reqs[i]->handle.Reset();

  // Let's wait for the rest to complete now.

  for (size_t i = kMaxSocketsPerGroup; i < arraysize(reqs); ++i) {
    EXPECT_EQ(OK, reqs[i]->WaitForResult());
    reqs[i]->handle.Reset();
  }

  EXPECT_EQ(kNumPendingRequests, TestSocketRequest::completion_count);
}

// Make sure that pending requests get serviced after active requests fail.
TEST_F(TCPClientSocketPoolTest, FailingActiveRequestWithPendingRequests) {
  client_socket_factory_.set_client_socket_type(
      MockClientSocketFactory::MOCK_PENDING_FAILING_CLIENT_SOCKET);

  scoped_ptr<TestSocketRequest> reqs[kMaxSocketsPerGroup * 2 + 1];

  // Queue up all the requests

  HostResolver::RequestInfo info("www.google.com", 80);
  for (size_t i = 0; i < arraysize(reqs); ++i) {
    reqs[i].reset(new TestSocketRequest(pool_.get(), &request_order_));
    int rv = reqs[i]->handle.Init("a", info, 5, reqs[i].get());
    EXPECT_EQ(ERR_IO_PENDING, rv);
  }

  for (size_t i = 0; i < arraysize(reqs); ++i)
    EXPECT_EQ(ERR_CONNECTION_FAILED, reqs[i]->WaitForResult());
}

}  // namespace

}  // namespace net
