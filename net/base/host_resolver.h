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
  // When callback is non-null, the operation will be performed asynchronously.
  // ERR_IO_PENDING is returned if it has been scheduled successfully. Real
  // result code will be passed to the completion callback.
  int Resolve(const std::string& hostname, int port,
              AddressList* addresses, CompletionCallback* callback);

 private:
  class Request;
  friend class Request;
  scoped_refptr<Request> request_;
  DISALLOW_COPY_AND_ASSIGN(HostResolver);
};

// A helper class used in unit tests to alter hostname mappings.  See
// SetHostMapper for details.
class HostMapper : public base::RefCountedThreadSafe<HostMapper> {
 public:
  virtual ~HostMapper() {}
  virtual std::string Map(const std::string& host) = 0;

 protected:
  // Ask previous host mapper (if set) for mapping of given host.
  std::string MapUsingPrevious(const std::string& host);

 private:
  friend class ScopedHostMapper;

  // Set mapper to ask when this mapper doesn't want to modify the result.
  void set_previous_mapper(HostMapper* mapper) {
    previous_mapper_ = mapper;
  }

  scoped_refptr<HostMapper> previous_mapper_;
};

#ifdef UNIT_TEST
// This function is designed to allow unit tests to override the behavior of
// HostResolver.  For example, a HostMapper instance can force all hostnames
// to map to a fixed IP address such as 127.0.0.1.
//
// The previously set HostMapper (or NULL if there was none) is returned.
//
// NOTE: This function is not thread-safe, so take care to only call this
// function while there are no outstanding HostResolver instances.
//
// NOTE: In most cases, you should use ScopedHostMapper instead, which is
// defined in host_resolver_unittest.h
//
HostMapper* SetHostMapper(HostMapper* host_mapper);
#endif

}  // namespace net

#endif  // NET_BASE_HOST_RESOLVER_H_
