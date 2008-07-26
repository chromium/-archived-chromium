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

#ifndef NET_HTTP_WINHTTP_REQUEST_THROTTLE_H_
#define NET_HTTP_WINHTTP_REQUEST_THROTTLE_H_

#include <windows.h>
#include <winhttp.h>

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/linked_ptr.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

namespace net {

// The WinHttpRequestThrottle class regulates the rate at which we call
// WinHttpSendRequest, ensuring that at any time there are at most 6 WinHTTP
// requests in progress for each server or proxy.
//
// The throttling is intended to cause WinHTTP to maintain at most 6
// persistent HTTP connections with each server or proxy.  This works well in
// most cases, except when making HTTPS requests via a proxy, in which case
// WinHTTP may open much more than 6 connections to the proxy in spite of our
// rate limiting.
//
// Because we identify a server by its hostname rather than its IP address,
// we also can't distinguish between two different hostnames that resolve to
// the same IP address.
//
// Although WinHTTP has the WINHTTP_OPTION_MAX_CONNS_PER_SERVER option to
// limit the number of connections allowed per server, we can't use it
// because it has two serious bugs:
// 1. It causes WinHTTP to not close idle persistent connections, leaving
//    many connections in the CLOSE_WAIT state.  This may cause system
//    crashes (Blue Screen of Death) when VPN is used.
// 2. It causes WinHTTP to crash intermittently in
//    HTTP_REQUEST_HANDLE_OBJECT::OpenProxyTunnel_Fsm() if a proxy is used.
// Therefore, we have to resort to throttling our WinHTTP requests to achieve
// the same effect.
//
// Note on thread safety: The WinHttpRequestThrottle class is only used by
// the IO thread, so it doesn't need to be protected with a lock.  The
// drawback is that the time we mark a request done is only approximate.
// We do that in the HttpTransactionWinHttp destructor, rather than in the
// WinHTTP status callback upon receiving HANDLE_CLOSING.
class WinHttpRequestThrottle {
 public:
  WinHttpRequestThrottle() {}

  virtual ~WinHttpRequestThrottle();

  // Intended to be a near drop-in replacement of WinHttpSendRequest.
  BOOL SubmitRequest(const std::string& server,
                     HINTERNET request_handle,
                     DWORD total_size,
                     DWORD_PTR context);

  // Called when a request failed or completed successfully.
  void NotifyRequestDone(const std::string& server);

  // Called from the HttpTransactionWinHttp destructor.
  void RemoveRequest(const std::string& server,
                     HINTERNET request_handle);

 protected:
  // Unit tests can stub out this method in a derived class.
  virtual BOOL SendRequest(HINTERNET request_handle,
                           DWORD total_size,
                           DWORD_PTR context,
                           bool report_async_error);

 private:
  FRIEND_TEST(WinHttpRequestThrottleTest, GarbageCollect);

  class RequestQueue;

  struct PerServerThrottle {
    PerServerThrottle();
    ~PerServerThrottle();

    int num_requests;  // Number of requests in progress
    linked_ptr<RequestQueue> request_queue;  // Requests waiting to be sent
  };

  typedef std::map<std::string, PerServerThrottle> ThrottleMap;

  static const int kMaxConnectionsPerServer;
  static const int kGarbageCollectionThreshold;

  void GarbageCollect();

  ThrottleMap throttles_;

  DISALLOW_EVIL_CONSTRUCTORS(WinHttpRequestThrottle);
};

}  // namespace net

#endif  // NET_HTTP_WINHTTP_REQUEST_THROTTLE_H_
