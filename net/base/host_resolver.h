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

#ifndef NET_BASE_HOST_RESOLVER_H_
#define NET_BASE_HOST_RESOLVER_H_

#include <string>

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "net/base/completion_callback.h"

namespace net {

class AddressList;

// This class represents the task of resolving a hostname (or IP address
// literal) to an AddressList object.  It can only resolve a single hostname at
// a time, so if you need to resolve multiple hostnames at the same time, you
// will need to allocate a HostResolver object for each hostname.
//
// No attempt is made at this level to cache or pin resolution results.  For
// each request, this API talks directly to the underlying name resolver of
// the local system, which may or may not result in a DNS query.  The exact
// behavior depends on the system configuration.
//
class HostResolver {
 public:
  HostResolver();

  // If a completion callback is pending when the resolver is destroyed, the
  // host resolution is cancelled, and the completion callback will not be
  // called.
  ~HostResolver();

  // Resolves the given hostname (or IP address literal), filling out the
  // |addresses| object upon success.  The |port| parameter will be set as the
  // sin(6)_port field of the sockaddr_in{6} struct.  Returns OK if successful
  // or an error code upon failure.
  //
  // When callback is non-null, ERR_IO_PENDING is returned if the operation
  // could not be completed synchronously, in which case the result code will
  // be passed to the callback when available.
  //
  int Resolve(const std::string& hostname, int port,
              AddressList* addresses, CompletionCallback* callback);

 private:
  class Request;
  friend class Request;
  scoped_refptr<Request> request_;
  DISALLOW_EVIL_CONSTRUCTORS(HostResolver);
};

}  // namespace net

#endif  // NET_BASE_HOST_RESOLVER_H_
