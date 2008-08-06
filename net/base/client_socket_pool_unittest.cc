// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "base/message_loop.h"
#include "net/base/client_socket.h"
#include "net/base/client_socket_handle.h"
#include "net/base/client_socket_pool.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

typedef testing::Test ClientSocketPoolTest;

const int kMaxSocketsPerGroup = 6;

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
  virtual int ReconnectIgnoringLastError(net::CompletionCallback* callback) {
    return net::ERR_FAILED;
  }
  virtual void Disconnect() {
    connected_ = false;
  }
  virtual bool IsConnected() const {
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
  TestSocketRequest(net::ClientSocketPool* pool) : handle(pool) {}

  net::ClientSocketHandle handle;

  void EnsureSocket() {
    DCHECK(handle.is_initialized());
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
};

int TestSocketRequest::completion_count = 0;

}  // namespace

TEST(ClientSocketPoolTest, Basic) {
  scoped_refptr<net::ClientSocketPool> pool =
      new net::ClientSocketPool(kMaxSocketsPerGroup);

  TestSocketRequest r(pool);
  int rv;

  rv = r.handle.Init("a", &r);
  EXPECT_EQ(net::OK, rv);
  EXPECT_TRUE(r.handle.is_initialized());

  r.handle.Reset();

  // The handle's Reset method may have posted a task.
  MessageLoop::current()->RunAllPending();
}

TEST(ClientSocketPoolTest, WithIdleConnection) {
  scoped_refptr<net::ClientSocketPool> pool =
      new net::ClientSocketPool(kMaxSocketsPerGroup);

  TestSocketRequest r(pool);
  int rv;

  rv = r.handle.Init("a", &r);
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

TEST(ClientSocketPoolTest, PendingRequests) {
  scoped_refptr<net::ClientSocketPool> pool =
      new net::ClientSocketPool(kMaxSocketsPerGroup);

  int rv;

  scoped_ptr<TestSocketRequest> reqs[kMaxSocketsPerGroup + 10];
  for (size_t i = 0; i < arraysize(reqs); ++i)
    reqs[i].reset(new TestSocketRequest(pool));

  // Reset
  MockClientSocket::allocation_count = 0;
  TestSocketRequest::completion_count = 0;

  // Create connections or queue up requests.
  for (size_t i = 0; i < arraysize(reqs); ++i) {
    rv = reqs[i]->handle.Init("a", reqs[i].get());
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
        reqs[i]->handle.Reset();
        MessageLoop::current()->RunAllPending();
        released_one = true;
      }
    }
  } while (released_one);

  EXPECT_EQ(kMaxSocketsPerGroup, MockClientSocket::allocation_count);
  EXPECT_EQ(10, TestSocketRequest::completion_count);
}

TEST(ClientSocketPoolTest, PendingRequests_NoKeepAlive) {
  scoped_refptr<net::ClientSocketPool> pool =
      new net::ClientSocketPool(kMaxSocketsPerGroup);

  int rv;

  scoped_ptr<TestSocketRequest> reqs[kMaxSocketsPerGroup + 10];
  for (size_t i = 0; i < arraysize(reqs); ++i)
    reqs[i].reset(new TestSocketRequest(pool));

  // Reset
  MockClientSocket::allocation_count = 0;
  TestSocketRequest::completion_count = 0;

  // Create connections or queue up requests.
  for (size_t i = 0; i < arraysize(reqs); ++i) {
    rv = reqs[i]->handle.Init("a", reqs[i].get());
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

  EXPECT_EQ(kMaxSocketsPerGroup + 10, MockClientSocket::allocation_count);
  EXPECT_EQ(10, TestSocketRequest::completion_count);
}

TEST(ClientSocketPoolTest, CancelRequest) {
  scoped_refptr<net::ClientSocketPool> pool =
      new net::ClientSocketPool(kMaxSocketsPerGroup);

  int rv;

  scoped_ptr<TestSocketRequest> reqs[kMaxSocketsPerGroup + 10];
  for (size_t i = 0; i < arraysize(reqs); ++i)
    reqs[i].reset(new TestSocketRequest(pool));

  // Reset
  MockClientSocket::allocation_count = 0;
  TestSocketRequest::completion_count = 0;

  // Create connections or queue up requests.
  for (size_t i = 0; i < arraysize(reqs); ++i) {
    rv = reqs[i]->handle.Init("a", reqs[i].get());
    if (rv != net::ERR_IO_PENDING) {
      EXPECT_EQ(net::OK, rv);
      reqs[i]->EnsureSocket();
    }
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
  EXPECT_EQ(9, TestSocketRequest::completion_count);
}
