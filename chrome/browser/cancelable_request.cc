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

#include "chrome/browser/cancelable_request.h"

CancelableRequestProvider::CancelableRequestProvider() : next_handle_(1) {
}

CancelableRequestProvider::~CancelableRequestProvider() {
  // There may be requests whose result callback has not been run yet. We need
  // to cancel them otherwise they may try and call us back after we've been
  // deleted, or do other bad things. This can occur on shutdown (or profile
  // destruction) when a request is scheduled, completed (but not dispatched),
  // then the Profile is deleted.
  AutoLock lock(pending_request_lock_);
  while (!pending_requests_.empty())
    CancelRequest(pending_requests_.begin()->first);
}

CancelableRequestProvider::Handle CancelableRequestProvider::AddRequest(
    CancelableRequestBase* request,
    CancelableRequestConsumerBase* consumer) {
  Handle handle;
  {
    AutoLock lock(pending_request_lock_);

    handle = next_handle_;
    pending_requests_[next_handle_] = request;
    ++next_handle_;
  }

  consumer->OnRequestAdded(this, handle);

  request->Init(this, handle, consumer);
  return handle;
}

void CancelableRequestProvider::CancelRequest(Handle handle) {
  AutoLock lock(pending_request_lock_);

  CancelableRequestMap::iterator i = pending_requests_.find(handle);
  if (i == pending_requests_.end()) {
    NOTREACHED() << "Trying to cancel an unknown request";
    return;
  }

  i->second->consumer()->OnRequestRemoved(this, handle);
  i->second->set_canceled();
  pending_requests_.erase(i);
}

void CancelableRequestProvider::RequestCompleted(Handle handle) {
  CancelableRequestConsumerBase* consumer;
  {
    AutoLock lock(pending_request_lock_);

    CancelableRequestMap::iterator i = pending_requests_.find(handle);
    if (i == pending_requests_.end()) {
      NOTREACHED() << "Trying to cancel an unknown request";
      return;
    }
    consumer = i->second->consumer();

    // This message should only get sent if the class is not cancelled, or
    // else the consumer might be gone).
    DCHECK(!i->second->canceled());

    pending_requests_.erase(i);
  }

  // Notify the consumer that the request is gone
  consumer->OnRequestRemoved(this, handle);
}
