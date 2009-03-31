// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "net/base/client_socket.h"
#include "net/base/client_socket_handle.h"
#include "net/base/client_socket_pool.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const int kMaxSocketsPerGroup = 6;

// Note that the first and the last are the same, the first should be handled
// before the last, since it was inserted first.
const int kPriorities[10] = { 1, 7, 9, 5, 6, 2, 8, 3, 4, 1 };

// This is the number of extra requests beyond the first few that use up all
// available sockets in the socket group.
const int kNumPendingRequests = arraysize(kPriorities);

class MockClientSocket : public net::ClientSocket {
 public:
  MockClientSocket() : connected_(false) {
    allocation_count++;
  }

  // ClientSocket methods:
  virtual int Connect(net::CompletionCallback* callback) {
    connected_ = true;
    return net::OK;
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
  virtual int Read(char* buf, int buf_len,
                   net::CompletionCallback* callback) {
    return net::ERR_FAILED;
  }
  virtual int Write(const char* buf, int buf_len,
                    net::CompletionCallback* callback) {
    return net::ERR_FAILED;
  }

  static int allocation_count;

 private:
  bool connected_;
};

int MockClientSocket::allocation_count = 0;

class TestSocketRequest : public CallbackRunner< Tuple1<int> > {
 public:
  TestSocketRequest(
      net::ClientSocketPool* pool,
      std::vector<TestSocketRequest*>* request_order)
      : handle(pool), request_order_(request_order) {}

  net::ClientSocketHandle handle;

  void EnsureSocket() {
    DCHECK(handle.is_initialized());
    request_order_->push_back(this);
    if (!handle.socket()) {
      handle.set_socket(new MockClientSocket());
      handle.socket()->Connect(NULL);
    }
  }

  virtual void RunWithParams(const Tuple1<int>& params) {
    DCHECK(params.a == net::OK);
    completion_count++;
    EnsureSocket();
  }

  static int completion_count;

 private:
  std::vector<TestSocketRequest*>* request_order_;
};

int TestSocketRequest::completion_count = 0;

class ClientSocketPoolTest : public testing::Test {
 protected:
  ClientSocketPoolTest()
      : pool_(new net::ClientSocketPool(kMaxSocketsPerGroup)) {}

  virtual void SetUp() {
    MockClientSocket::allocation_count = 0;
    TestSocketRequest::completion_count = 0;
  }

  scoped_refptr<net::ClientSocketPool> pool_;
  std::vector<TestSocketRequest*> request_order_;
};

TEST_F(ClientSocketPoolTest, Basic) {
  TestSocketRequest r(pool_.get(), &request_order_);
  int rv;

  rv = r.handle.Init("a", 0, &r);
  EXPECT_EQ(net::OK, rv);
  EXPECT_TRUE(r.handle.is_initialized());

  r.handle.Reset();

  // The handle's Reset method may have posted a task.
  MessageLoop::current()->RunAllPending();
}

TEST_F(ClientSocketPoolTest, WithIdleConnection) {
  TestSocketRequest r(pool_.get(), &request_order_);
  int rv;

  rv = r.handle.Init("a", 0, &r);
  EXPECT_EQ(net::OK, rv);
  EXPECT_TRUE(r.handle.is_initialized());

  // Create a socket.
  r.EnsureSocket();

  // Release the socket.  It should find its way into the idle list.  We're
  // testing that this does not trigger a crash.
  r.handle.Reset();

  // The handle's Reset method may have posted a task.
  MessageLoop::current()->RunAllPending();
}

TEST_F(ClientSocketPoolTest, PendingRequests) {
  int rv;

  scoped_ptr<TestSocketRequest> reqs[kMaxSocketsPerGroup + kNumPendingRequests];

  for (size_t i = 0; i < arraysize(reqs); ++i)
    reqs[i].reset(new TestSocketRequest(pool_.get(), &request_order_));

  // Create connections or queue up requests.
  for (int i = 0; i < kMaxSocketsPerGroup; ++i) {
    rv = reqs[i]->handle.Init("a", 5, reqs[i].get());
    EXPECT_EQ(net::OK, rv);
    reqs[i]->EnsureSocket();
  }
  for (int i = 0; i < kNumPendingRequests; ++i) {
    rv = reqs[kMaxSocketsPerGroup + i]->handle.Init(
        "a", kPriorities[i], reqs[kMaxSocketsPerGroup + i].get());
    EXPECT_EQ(net::ERR_IO_PENDING, rv);
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

  EXPECT_EQ(kMaxSocketsPerGroup, MockClientSocket::allocation_count);
  EXPECT_EQ(kNumPendingRequests, TestSocketRequest::completion_count);

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
  int rv;

  scoped_ptr<TestSocketRequest> reqs[kMaxSocketsPerGroup + kNumPendingRequests];
  for (size_t i = 0; i < arraysize(reqs); ++i)
    reqs[i].reset(new TestSocketRequest(pool_.get(), &request_order_));

  // Create connections or queue up requests.
  for (size_t i = 0; i < arraysize(reqs); ++i) {
    rv = reqs[i]->handle.Init("a", 0, reqs[i].get());
    if (rv != net::ERR_IO_PENDING) {
      EXPECT_EQ(net::OK, rv);
      reqs[i]->EnsureSocket();
    }
  }

  // Release any connections until we have no connections.
  bool released_one;
  do {
    released_one = false;
    for (size_t i = 0; i < arraysize(reqs); ++i) {
      if (reqs[i]->handle.is_initialized()) {
        reqs[i]->handle.socket()->Disconnect();
        reqs[i]->handle.Reset();
        MessageLoop::current()->RunAllPending();
        released_one = true;
      }
    }
  } while (released_one);

  EXPECT_EQ(kMaxSocketsPerGroup + kNumPendingRequests,
            MockClientSocket::allocation_count);
  EXPECT_EQ(kNumPendingRequests, TestSocketRequest::completion_count);
}

TEST_F(ClientSocketPoolTest, CancelRequest) {
  int rv;

  scoped_ptr<TestSocketRequest> reqs[kMaxSocketsPerGroup + kNumPendingRequests];

  for (size_t i = 0; i < arraysize(reqs); ++i)
    reqs[i].reset(new TestSocketRequest(pool_.get(), &request_order_));

  // Create connections or queue up requests.
  for (int i = 0; i < kMaxSocketsPerGroup; ++i) {
    rv = reqs[i]->handle.Init("a", 5, reqs[i].get());
    EXPECT_EQ(net::OK, rv);
    reqs[i]->EnsureSocket();
  }
  for (int i = 0; i < kNumPendingRequests; ++i) {
    rv = reqs[kMaxSocketsPerGroup + i]->handle.Init(
        "a", kPriorities[i], reqs[kMaxSocketsPerGroup + i].get());
    EXPECT_EQ(net::ERR_IO_PENDING, rv);
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

  EXPECT_EQ(kMaxSocketsPerGroup, MockClientSocket::allocation_count);
  EXPECT_EQ(kNumPendingRequests - 1, TestSocketRequest::completion_count);
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
