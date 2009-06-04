// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "net/base/client_socket.h"
#include "net/base/client_socket_factory.h"
#include "net/base/client_socket_handle.h"
#include "net/base/client_socket_pool.h"
#include "net/base/host_resolver_unittest.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
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
  MockPendingClientSocket() {}

  // ClientSocket methods:
  virtual int Connect(CompletionCallback* callback) {
    return ERR_IO_PENDING;
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

class MockClientSocketFactory : public ClientSocketFactory {
 public:
  enum ClientSocketType {
    MOCK_CLIENT_SOCKET,
    MOCK_FAILING_CLIENT_SOCKET,
    MOCK_PENDING_CLIENT_SOCKET,
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
        return new MockPendingClientSocket();
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

class ClientSocketPoolTest : public testing::Test {
 protected:
  ClientSocketPoolTest()
      : pool_(new ClientSocketPool(kMaxSocketsPerGroup,
                                   &client_socket_factory_)) {}

  virtual void SetUp() {
    TestSocketRequest::completion_count = 0;
  }

  MockClientSocketFactory client_socket_factory_;
  scoped_refptr<ClientSocketPool> pool_;
  std::vector<TestSocketRequest*> request_order_;
};

TEST_F(ClientSocketPoolTest, Basic) {
  TestCompletionCallback callback;
  ClientSocketHandle handle(pool_.get());
  int rv = handle.Init("a", "www.google.com", 80, 0, &callback);
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  EXPECT_EQ(OK, callback.WaitForResult());
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());

  handle.Reset();

  // The handle's Reset method may have posted a task.
  MessageLoop::current()->RunAllPending();
}

TEST_F(ClientSocketPoolTest, InitHostResolutionFailure) {
  RuleBasedHostMapper* host_mapper = new RuleBasedHostMapper;
  host_mapper->AddSimulatedFailure("unresolvable.host.name");
  ScopedHostMapper scoped_host_mapper(host_mapper);
  TestSocketRequest req(pool_.get(), &request_order_);
  EXPECT_EQ(ERR_IO_PENDING,
            req.handle.Init("a", "unresolvable.host.name", 80, 5, &req));
  EXPECT_EQ(ERR_NAME_NOT_RESOLVED, req.WaitForResult());
}

TEST_F(ClientSocketPoolTest, InitConnectionFailure) {
  client_socket_factory_.set_client_socket_type(
      MockClientSocketFactory::MOCK_FAILING_CLIENT_SOCKET);
  TestSocketRequest req(pool_.get(), &request_order_);
  EXPECT_EQ(ERR_IO_PENDING,
            req.handle.Init("a", "unresolvable.host.name", 80, 5, &req));
  EXPECT_EQ(ERR_CONNECTION_FAILED, req.WaitForResult());
}

TEST_F(ClientSocketPoolTest, PendingRequests) {
  int rv;

  scoped_ptr<TestSocketRequest> reqs[kNumRequests];

  for (size_t i = 0; i < arraysize(reqs); ++i)
    reqs[i].reset(new TestSocketRequest(pool_.get(), &request_order_));

  // Create connections or queue up requests.
  for (int i = 0; i < kMaxSocketsPerGroup; ++i) {
    rv = reqs[i]->handle.Init("a", "www.google.com", 80, 5, reqs[i].get());
    EXPECT_EQ(ERR_IO_PENDING, rv);
    EXPECT_EQ(OK, reqs[i]->WaitForResult());
  }

  for (int i = 0; i < kNumPendingRequests; ++i) {
    rv = reqs[kMaxSocketsPerGroup + i]->handle.Init(
        "a", "www.google.com", 80, kPriorities[i],
        reqs[kMaxSocketsPerGroup + i].get());
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
  EXPECT_EQ(kNumRequests, TestSocketRequest::completion_count);

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

TEST_F(ClientSocketPoolTest, PendingRequests_NoKeepAlive) {
  scoped_ptr<TestSocketRequest> reqs[kNumRequests];
  for (size_t i = 0; i < arraysize(reqs); ++i)
    reqs[i].reset(new TestSocketRequest(pool_.get(), &request_order_));

  // Create connections or queue up requests.
  for (int i = 0; i < kMaxSocketsPerGroup; ++i) {
    int rv = reqs[i]->handle.Init("a", "www.google.com", 80, 0, reqs[i].get());
    EXPECT_EQ(ERR_IO_PENDING, rv);
    EXPECT_EQ(OK, reqs[i]->WaitForResult());
  }

  for (int i = 0; i < kNumPendingRequests; ++i) {
    EXPECT_EQ(ERR_IO_PENDING, reqs[kMaxSocketsPerGroup + i]->handle.Init(
        "a", "www.google.com", 80, 0, reqs[kMaxSocketsPerGroup + i].get()));
  }

  // Release any connections until we have no connections.

  while (TestSocketRequest::completion_count < kNumRequests) {
    int num_released = 0;
    for (size_t i = 0; i < arraysize(reqs); ++i) {
      if (reqs[i]->handle.is_initialized()) {
        reqs[i]->handle.socket()->Disconnect();
        reqs[i]->handle.Reset();
        num_released++;
      }
    }
    int curr_num_completed = TestSocketRequest::completion_count;
    for (int i = 0;
         (i < num_released) && (i + curr_num_completed < kNumRequests); ++i) {
      EXPECT_EQ(OK, reqs[i + curr_num_completed]->WaitForResult());
    }
  }

  EXPECT_EQ(kNumRequests, client_socket_factory_.allocation_count());
  EXPECT_EQ(kNumRequests, TestSocketRequest::completion_count);
}

// This test will start up a RequestSocket() and then immediately Cancel() it.
// The pending host resolution will eventually complete, and destroy the
// ClientSocketPool which will crash if the group was not cleared properly.
TEST_F(ClientSocketPoolTest, CancelRequestClearGroup) {
  TestSocketRequest req(pool_.get(), &request_order_);
  EXPECT_EQ(ERR_IO_PENDING,
            req.handle.Init("a", "www.google.com", 80, 5, &req));
  req.handle.Reset();

  PlatformThread::Sleep(100);

  // There is a race condition here.  If the worker pool doesn't post the task
  // before we get here, then this might not run ConnectingSocket::OnIOComplete
  // and therefore leak the canceled ConnectingSocket.  However, other tests
  // after this will call MessageLoop::RunAllPending() which should prevent a
  // leak, unless the worker thread takes longer than all of them.
  MessageLoop::current()->RunAllPending();
}

TEST_F(ClientSocketPoolTest, TwoRequestsCancelOne) {
  TestSocketRequest req(pool_.get(), &request_order_);
  TestSocketRequest req2(pool_.get(), &request_order_);

  EXPECT_EQ(ERR_IO_PENDING,
            req.handle.Init("a", "www.google.com", 80, 5, &req));
  EXPECT_EQ(ERR_IO_PENDING,
            req2.handle.Init("a", "www.google.com", 80, 5, &req));

  req.handle.Reset();
  PlatformThread::Sleep(100);

  // There is a benign race condition here.  The worker pool may or may not post
  // the tasks before we get here.  It won't test the case properly if it
  // doesn't, but 100ms should be enough most of the time.
  MessageLoop::current()->RunAllPending();

  req2.handle.Reset();
  // The handle's Reset method may have posted a task.
  MessageLoop::current()->RunAllPending();
}

TEST_F(ClientSocketPoolTest, ConnectCancelConnect) {
  client_socket_factory_.set_client_socket_type(
      MockClientSocketFactory::MOCK_PENDING_CLIENT_SOCKET);
  TestSocketRequest req(pool_.get(), &request_order_);

  EXPECT_EQ(ERR_IO_PENDING,
            req.handle.Init("a", "www.google.com", 80, 5, &req));

  req.handle.Reset();

  EXPECT_EQ(ERR_IO_PENDING,
            req.handle.Init("a", "www.google.com", 80, 5, &req));

  // There is a benign race condition here.  The worker pool may or may not post
  // the tasks before we get here.  It won't test the case properly if it
  // doesn't, but 100ms should be enough most of the time.

  // Let the first ConnectingSocket for the handle run.  This should have been
  // canceled, so it shouldn't update the state of any Request.
  PlatformThread::Sleep(100);
  MessageLoop::current()->RunAllPending();

  // Let the second ConnectingSocket for the handle run.  If the first
  // ConnectingSocket updated the state of any request, this will crash.
  PlatformThread::Sleep(100);
  MessageLoop::current()->RunAllPending();

  req.handle.Reset();
  // The handle's Reset method may have posted a task.
  MessageLoop::current()->RunAllPending();
}

TEST_F(ClientSocketPoolTest, CancelRequest) {
  scoped_ptr<TestSocketRequest> reqs[kNumRequests];

  for (size_t i = 0; i < arraysize(reqs); ++i)
    reqs[i].reset(new TestSocketRequest(pool_.get(), &request_order_));

  // Create connections or queue up requests.
  for (int i = 0; i < kMaxSocketsPerGroup; ++i) {
    int rv = reqs[i]->handle.Init("a", "www.google.com", 80, 5, reqs[i].get());
    EXPECT_EQ(ERR_IO_PENDING, rv);
    EXPECT_EQ(OK, reqs[i]->WaitForResult());
  }

  for (int i = 0; i < kNumPendingRequests; ++i) {
    int rv = reqs[kMaxSocketsPerGroup + i]->handle.Init(
        "a", "www.google.com", 80, kPriorities[i],
        reqs[kMaxSocketsPerGroup + i].get());
    EXPECT_EQ(ERR_IO_PENDING, rv);
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
  EXPECT_EQ(kNumRequests - 1, TestSocketRequest::completion_count);
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

}  // namespace

}  // namespace net
