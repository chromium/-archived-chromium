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
#include "net/base/net_errors.h"
#include "net/http/http_connection_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

typedef testing::Test HttpConnectionManagerTest;

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
  TestSocketRequest() : handle(NULL) {}

  net::HttpConnectionManager::SocketHandle* handle;

  void InitHandle() {
    DCHECK(handle);
    if (!handle->get()) {
      handle->reset(new MockClientSocket());
      handle->get()->Connect(NULL);
    }
  }

  virtual void RunWithParams(const Tuple1<int>& params) {
    DCHECK(params.a == net::OK);
    completion_count++;
    InitHandle();
  }

  static int completion_count;
};

int TestSocketRequest::completion_count = 0;

// Call ReleaseSocket and wait for it to complete.  It runs via PostTask, so we
// can just empty the MessageLoop to ensure that ReleaseSocket finished.
void CallReleaseSocket(net::HttpConnectionManager* connection_mgr,
                       const std::string& group_name,
                       net::HttpConnectionManager::SocketHandle* handle) {
  connection_mgr->ReleaseSocket(group_name, handle);
  MessageLoop::current()->Quit();
  MessageLoop::current()->Run();
}

}  // namespace

TEST(HttpConnectionManagerTest, Basic) {
  scoped_refptr<net::HttpConnectionManager> mgr =
      new net::HttpConnectionManager();

  TestSocketRequest r;
  int rv;

  rv = mgr->RequestSocket("a", &r.handle, &r);
  EXPECT_EQ(net::OK, rv);
  EXPECT_TRUE(r.handle != NULL);

  CallReleaseSocket(mgr, "a", r.handle);
}

TEST(HttpConnectionManagerTest, WithIdleConnection) {
  scoped_refptr<net::HttpConnectionManager> mgr =
      new net::HttpConnectionManager();

  TestSocketRequest r;
  int rv;

  rv = mgr->RequestSocket("a", &r.handle, &r);
  EXPECT_EQ(net::OK, rv);
  EXPECT_TRUE(r.handle != NULL);

  r.InitHandle();

  CallReleaseSocket(mgr, "a", r.handle);
}

TEST(HttpConnectionManagerTest, PendingRequests) {
  scoped_refptr<net::HttpConnectionManager> mgr =
      new net::HttpConnectionManager();

  int rv;

  TestSocketRequest reqs[
      net::HttpConnectionManager::kMaxSocketsPerGroup + 10];

  // Reset
  MockClientSocket::allocation_count = 0;
  TestSocketRequest::completion_count = 0;

  // Create connections or queue up requests.
  for (size_t i = 0; i < arraysize(reqs); ++i) {
    rv = mgr->RequestSocket("a", &reqs[i].handle, &reqs[i]);
    if (rv != net::ERR_IO_PENDING) {
      EXPECT_EQ(net::OK, rv);
      reqs[i].InitHandle();
    }
  }

  // Release any connections until we have no connections.
  bool released_one;
  do {
    released_one = false;
    for (size_t i = 0; i < arraysize(reqs); ++i) {
      if (reqs[i].handle) {
        CallReleaseSocket(mgr, "a", reqs[i].handle);
        reqs[i].handle = NULL;
        released_one = true;
      }
    }
  } while (released_one);

  EXPECT_EQ(net::HttpConnectionManager::kMaxSocketsPerGroup,
            MockClientSocket::allocation_count);
  EXPECT_EQ(10, TestSocketRequest::completion_count);
}

TEST(HttpConnectionManagerTest, PendingRequests_NoKeepAlive) {
  scoped_refptr<net::HttpConnectionManager> mgr =
      new net::HttpConnectionManager();

  int rv;

  TestSocketRequest reqs[
      net::HttpConnectionManager::kMaxSocketsPerGroup + 10];

  // Reset
  MockClientSocket::allocation_count = 0;
  TestSocketRequest::completion_count = 0;

  // Create connections or queue up requests.
  for (size_t i = 0; i < arraysize(reqs); ++i) {
    rv = mgr->RequestSocket("a", &reqs[i].handle, &reqs[i]);
    if (rv != net::ERR_IO_PENDING) {
      EXPECT_EQ(net::OK, rv);
      reqs[i].InitHandle();
    }
  }

  // Release any connections until we have no connections.
  bool released_one;
  do {
    released_one = false;
    for (size_t i = 0; i < arraysize(reqs); ++i) {
      if (reqs[i].handle) {
        reqs[i].handle->get()->Disconnect();
        CallReleaseSocket(mgr, "a", reqs[i].handle);
        reqs[i].handle = NULL;
        released_one = true;
      }
    }
  } while (released_one);

  EXPECT_EQ(net::HttpConnectionManager::kMaxSocketsPerGroup + 10,
            MockClientSocket::allocation_count);
  EXPECT_EQ(10, TestSocketRequest::completion_count);
}

TEST(HttpConnectionManagerTest, CancelRequest) {
  scoped_refptr<net::HttpConnectionManager> mgr =
      new net::HttpConnectionManager();

  int rv;

  TestSocketRequest reqs[
      net::HttpConnectionManager::kMaxSocketsPerGroup + 10];

  // Reset
  MockClientSocket::allocation_count = 0;
  TestSocketRequest::completion_count = 0;

  // Create connections or queue up requests.
  for (size_t i = 0; i < arraysize(reqs); ++i) {
    rv = mgr->RequestSocket("a", &reqs[i].handle, &reqs[i]);
    if (rv != net::ERR_IO_PENDING) {
      EXPECT_EQ(net::OK, rv);
      reqs[i].InitHandle();
    }
  }

  // Cancel a request.
  size_t index_to_cancel =
      net::HttpConnectionManager::kMaxSocketsPerGroup + 2;
  EXPECT_TRUE(reqs[index_to_cancel].handle == NULL);
  mgr->CancelRequest("a", &reqs[index_to_cancel].handle);

  // Release any connections until we have no connections.
  bool released_one;
  do {
    released_one = false;
    for (size_t i = 0; i < arraysize(reqs); ++i) {
      if (reqs[i].handle) {
        CallReleaseSocket(mgr, "a", reqs[i].handle);
        reqs[i].handle = NULL;
        released_one = true;
      }
    }
  } while (released_one);

  EXPECT_EQ(net::HttpConnectionManager::kMaxSocketsPerGroup,
            MockClientSocket::allocation_count);
  EXPECT_EQ(9, TestSocketRequest::completion_count);
}
