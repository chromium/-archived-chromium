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

#include "net/http/winhttp_request_throttle.h"

#include "base/logging.h"
#include "net/http/http_transaction_winhttp.h"

namespace {

// The arguments to a WinHttpSendRequest call.
struct SendRequestArgs {
  SendRequestArgs() : request_handle(NULL), total_size(0), context(0) {}

  SendRequestArgs(HINTERNET handle, DWORD size, DWORD_PTR context_value)
      : request_handle(handle), total_size(size), context(context_value) {}

  HINTERNET request_handle;
  DWORD total_size;
  DWORD_PTR context;
};

}  // namespace

namespace net {

// Per-server queue for WinHttpSendRequest calls.
class WinHttpRequestThrottle::RequestQueue {
 public:
  RequestQueue() {}

  // Adds |args| to the end of the queue.
  void PushBack(const SendRequestArgs& args) { queue_.push_back(args); }

  // If the queue is not empty, pops the first entry off the queue, saves it
  // in |*args|, and returns true.  If the queue is empty, returns false.
  bool GetFront(SendRequestArgs* args);

  // If the queue has an entry containing |request_handle|, removes it and
  // returns true.  Otherwise, returns false.
  bool Remove(HINTERNET request_handle);

  bool empty() const { return queue_.empty(); }

 private:
  std::list<SendRequestArgs> queue_;

  DISALLOW_EVIL_CONSTRUCTORS(RequestQueue);
};

bool WinHttpRequestThrottle::RequestQueue::GetFront(SendRequestArgs* args) {
  if (queue_.empty())
    return false;
  *args = queue_.front();
  queue_.pop_front();
  return true;
}

bool WinHttpRequestThrottle::RequestQueue::Remove(HINTERNET request_handle) {
  std::list<SendRequestArgs>::iterator it;
  for (it = queue_.begin(); it != queue_.end(); ++it) {
    if (it->request_handle == request_handle) {
      queue_.erase(it);
      return true;
    }
  }
  return false;
}

WinHttpRequestThrottle::~WinHttpRequestThrottle() {
#ifndef NDEBUG
  ThrottleMap::const_iterator throttle_iter = throttles_.begin();
  for (; throttle_iter != throttles_.end(); ++throttle_iter) {
    const PerServerThrottle& throttle = throttle_iter->second;
    DCHECK(throttle.num_requests == 0);
    DCHECK(!throttle.request_queue.get() || throttle.request_queue->empty());
  }
#endif
}

BOOL WinHttpRequestThrottle::SubmitRequest(const std::string &server,
                                           HINTERNET request_handle,
                                           DWORD total_size,
                                           DWORD_PTR context) {
  PerServerThrottle& throttle = throttles_[server];
  DCHECK(throttle.num_requests >= 0 &&
         throttle.num_requests <= kMaxConnectionsPerServer);
  if (throttle.num_requests >= kMaxConnectionsPerServer) {
    if (!throttle.request_queue.get())
      throttle.request_queue.reset(new RequestQueue);
    SendRequestArgs args(request_handle, total_size, context);
    throttle.request_queue->PushBack(args);
    return TRUE;
  }

  BOOL ok = SendRequest(request_handle, total_size, context, false);
  if (ok)
    throttle.num_requests += 1;
  return ok;
}

void WinHttpRequestThrottle::NotifyRequestDone(const std::string& server) {
  PerServerThrottle& throttle = throttles_[server];
  DCHECK(throttle.num_requests > 0 &&
         throttle.num_requests <= kMaxConnectionsPerServer);
  throttle.num_requests -= 1;
  SendRequestArgs args;
  if (throttle.request_queue.get() &&
      throttle.request_queue->GetFront(&args)) {
    throttle.num_requests += 1;
    SendRequest(args.request_handle, args.total_size, args.context, true);
  }
  if (throttles_.size() > static_cast<size_t>(kGarbageCollectionThreshold))
    GarbageCollect();
}

void WinHttpRequestThrottle::RemoveRequest(const std::string& server,
                                           HINTERNET request_handle) {
  PerServerThrottle& throttle = throttles_[server];
  if (throttle.request_queue.get() &&
      throttle.request_queue->Remove(request_handle))
    return;
  NotifyRequestDone(server);
}

BOOL WinHttpRequestThrottle::SendRequest(HINTERNET request_handle,
                                         DWORD total_size,
                                         DWORD_PTR context,
                                         bool report_async_error) {
  BOOL ok = WinHttpSendRequest(request_handle,
                               WINHTTP_NO_ADDITIONAL_HEADERS,
                               0,
                               WINHTTP_NO_REQUEST_DATA,
                               0,
                               total_size,
                               context);
  if (!ok && report_async_error) {
    WINHTTP_ASYNC_RESULT async_result = { API_SEND_REQUEST, GetLastError() };
    HttpTransactionWinHttp::StatusCallback(
        request_handle, context,
        WINHTTP_CALLBACK_STATUS_REQUEST_ERROR,
        &async_result, sizeof(async_result));
  }
  return ok;
}

WinHttpRequestThrottle::PerServerThrottle::PerServerThrottle()
    : num_requests(0) {
}

WinHttpRequestThrottle::PerServerThrottle::~PerServerThrottle() {
}

// static
const int WinHttpRequestThrottle::kMaxConnectionsPerServer = 6;

// static
const int WinHttpRequestThrottle::kGarbageCollectionThreshold = 64;

void WinHttpRequestThrottle::GarbageCollect() {
  ThrottleMap::iterator throttle_iter = throttles_.begin();
  while (throttle_iter != throttles_.end()) {
    PerServerThrottle& throttle = throttle_iter->second;
    if (throttle.num_requests == 0 &&
       (!throttle.request_queue.get() || throttle.request_queue->empty())) {
      // Erase the current item but keep the iterator valid.
      throttles_.erase(throttle_iter++);
    } else {
      ++throttle_iter;
    }
  }
}

}  // namespace net
