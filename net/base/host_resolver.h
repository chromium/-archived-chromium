// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
  // When callback is null, the operation completes synchronously.
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

