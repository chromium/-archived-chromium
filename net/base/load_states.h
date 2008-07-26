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

#ifndef NET_BASE_LOAD_STATES_H__
#define NET_BASE_LOAD_STATES_H__

namespace net {

// These states correspond to the lengthy periods of time that a resource load
// may be blocked and unable to make progress.
enum LoadState {
  // This is the default state.  It corresponds to a resource load that has
  // either not yet begun or is idle waiting for the consumer to do something
  // to move things along (e.g., the consumer of an URLRequest may not have
  // called Read yet).
  LOAD_STATE_IDLE,

  // This state corresponds to a resource load that is blocked waiting for
  // access to a resource in the cache.  If multiple requests are made for the
  // same resource, the first request will be responsible for writing (or
  // updating) the cache entry and the second request will be deferred until
  // the first completes.  This may be done to optimize for cache reuse.
  LOAD_STATE_WAITING_FOR_CACHE,

  // This state corresponds to a resource load that is blocked waiting for a
  // proxy autoconfig script to return a proxy server to use.  This state may
  // take a while if the proxy script needs to resolve the IP address of the
  // host before deciding what proxy to use.
  LOAD_STATE_RESOLVING_PROXY_FOR_URL,

  // This state corresponds to a resource load that is blocked waiting for a
  // host name to be resolved.  This could either indicate resolution of the
  // origin server corresponding to the resource or to the host name of a proxy
  // server used to fetch the resource.
  LOAD_STATE_RESOLVING_HOST,

  // This state corresponds to a resource load that is blocked waiting for a
  // TCP connection (or other network connection) to be established.  HTTP
  // requests that reuse a keep-alive connection skip this state.
  LOAD_STATE_CONNECTING,

  // This state corresponds to a resource load that is blocked waiting to
  // completely upload a request to a server.  In the case of a HTTP POST
  // request, this state includes the period of time during which the message
  // body is being uploaded.
  LOAD_STATE_SENDING_REQUEST,

  // This state corresponds to a resource load that is blocked waiting for the
  // response to a network request.  In the case of a HTTP transaction, this
  // corresponds to the period after the request is sent and before all of the
  // response headers have been received.
  LOAD_STATE_WAITING_FOR_RESPONSE,

  // This state corresponds to a resource load that is blocked waiting for a
  // read to complete.  In the case of a HTTP transaction, this corresponds to
  // the period after the response headers have been received and before all of
  // the response body has been downloaded.  (NOTE: This state only applies for
  // an URLRequest while there is an outstanding Read operation.)
  LOAD_STATE_READING_RESPONSE,
};

}  // namespace net

#endif  // NET_BASE_LOAD_STATES_H__
