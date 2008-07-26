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

#include "base/string_util.h"
#include "net/http/winhttp_request_throttle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Converts an int i to an HINTERNET (void *) request handle.
HINTERNET RequestHandle(int i) {
  return reinterpret_cast<HINTERNET>(static_cast<intptr_t>(i));
}

class MockRequestThrottle : public net::WinHttpRequestThrottle {
 public:
  MockRequestThrottle() : last_sent_request_(NULL) { }

  // The request handle of the last sent request.  This allows us to determine
  // whether a submitted request was sent or queued.
  HINTERNET last_sent_request() const { return last_sent_request_; }

 protected:
  virtual BOOL SendRequest(HINTERNET request_handle,
                           DWORD total_size,
                           DWORD_PTR context,
                           bool report_async_error) {
    last_sent_request_ = request_handle;
    return TRUE;
  }

 private:
  HINTERNET last_sent_request_;

  DISALLOW_EVIL_CONSTRUCTORS(MockRequestThrottle);
};

}  // namespace

namespace net {

TEST(WinHttpRequestThrottleTest, OneServer) {
  MockRequestThrottle throttle;
  std::string server("http://www.foo.com");
  HINTERNET request_handle;

  // Submit 20 requests to the request throttle.
  // Expected outcome: 6 requests should be in progress, and requests 7-20
  // should be queued.
  for (int i = 1; i <= 20; i++) {
    request_handle = RequestHandle(i);
    EXPECT_TRUE(throttle.SubmitRequest(server, request_handle, 0, 0));
    if (i <= 6)
      EXPECT_EQ(request_handle, throttle.last_sent_request());
    else
      EXPECT_EQ(RequestHandle(6), throttle.last_sent_request());
  }

  // Notify the request throttle of the completion of 10 requests.
  // Expected outcome: 6 requests should be in progress, and requests 17-20
  // should be queued.
  for (int j = 0; j < 10; j++) {
    throttle.NotifyRequestDone(server);
    EXPECT_EQ(RequestHandle(7 + j), throttle.last_sent_request());
  }

  // Remove request 17, which is queued.
  // Expected outcome: Requests 18-20 should remain queued.
  request_handle = RequestHandle(17);
  throttle.RemoveRequest(server, request_handle);
  EXPECT_EQ(RequestHandle(16), throttle.last_sent_request());

  // Remove request 16, which is in progress.
  // Expected outcome: The request throttle should send request 18.
  // Requests 19-20 should remained queued.
  request_handle = RequestHandle(16);
  throttle.RemoveRequest(server, request_handle);
  EXPECT_EQ(RequestHandle(18), throttle.last_sent_request());

  // Notify the request throttle of the completion of the remaining
  // 8 requests.
  for (int j = 0; j < 8; j++) {
    throttle.NotifyRequestDone(server);
    if (j < 2)
      EXPECT_EQ(RequestHandle(19 + j), throttle.last_sent_request());
    else
      EXPECT_EQ(RequestHandle(20), throttle.last_sent_request());
  }
}

// Submit requests to a large number (> 64) of servers to force the garbage
// collection of idle PerServerThrottles.
TEST(WinHttpRequestThrottleTest, GarbageCollect) {
  MockRequestThrottle throttle;
  for (int i = 0; i < 150; i++) {
    std::string server("http://www.foo");
    server.append(IntToString(i));
    server.append(".com");
    throttle.SubmitRequest(server, RequestHandle(1), 0, 0);
    throttle.NotifyRequestDone(server);
    if (i < 64)
      EXPECT_EQ(i + 1, throttle.throttles_.size());
    else if (i < 129)
      EXPECT_EQ(i - 64, throttle.throttles_.size());
    else
      EXPECT_EQ(i - 129, throttle.throttles_.size());
  }
}

}  // namespace net
