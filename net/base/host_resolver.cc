// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/host_resolver.h"

#if defined(OS_WIN)
#include <ws2tcpip.h>
#include <wspiapi.h>  // Needed for Win2k compat.
#elif defined(OS_POSIX)
#include <netdb.h>
#include <sys/socket.h>
#endif

#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/worker_pool.h"
#include "net/base/address_list.h"
#include "net/base/net_errors.h"

#if defined(OS_WIN)
#include "net/base/winsock_init.h"
#endif

namespace net {

//-----------------------------------------------------------------------------

static HostMapper* host_mapper;

HostMapper* SetHostMapper(HostMapper* value) {
  std::swap(host_mapper, value);
  return value;
}

static int HostResolverProc(
    const std::string& host, const std::string& port, struct addrinfo** out) {
  struct addrinfo hints = {0};
  hints.ai_family = PF_UNSPEC;
  hints.ai_flags = AI_ADDRCONFIG;

  // Restrict result set to only this socket type to avoid duplicates.
  hints.ai_socktype = SOCK_STREAM;

  int err = getaddrinfo(host.c_str(), port.c_str(), &hints, out);
  return err ? ERR_NAME_NOT_RESOLVED : OK;
}

static int ResolveAddrInfo(
    const std::string& host, const std::string& port, struct addrinfo** out) {
  int rv;
  if (host_mapper) {
    rv = HostResolverProc(host_mapper->Map(host), port, out);
  } else {
    rv = HostResolverProc(host, port, out);
  }
  return rv;
}

//-----------------------------------------------------------------------------

class HostResolver::Request :
    public base::RefCountedThreadSafe<HostResolver::Request> {
 public:
  Request(HostResolver* resolver,
          const std::string& host,
          const std::string& port,
          AddressList* addresses,
          CompletionCallback* callback)
      : host_(host),
        port_(port),
        resolver_(resolver),
        addresses_(addresses),
        callback_(callback),
        origin_loop_(MessageLoop::current()),
        error_(OK),
        results_(NULL) {
  }

  ~Request() {
    if (results_)
      freeaddrinfo(results_);
  }

  void DoLookup() {
    // Running on the worker thread
    error_ = ResolveAddrInfo(host_, port_, &results_);

    Task* reply = NewRunnableMethod(this, &Request::DoCallback);

    // The origin loop could go away while we are trying to post to it, so we
    // need to call its PostTask method inside a lock.  See ~HostResolver.
    {
      AutoLock locked(origin_loop_lock_);
      if (origin_loop_) {
        origin_loop_->PostTask(FROM_HERE, reply);
        reply = NULL;
      }
    }

    // Does nothing if it got posted.
    delete reply;
  }

  void DoCallback() {
    // Running on the origin thread.
    DCHECK(error_ || results_);

    // We may have been cancelled!
    if (!resolver_)
      return;

    if (!error_) {
      addresses_->Adopt(results_);
      results_ = NULL;
    }

    // Drop the resolver's reference to us.  Do this before running the
    // callback since the callback might result in the resolver being
    // destroyed.
    resolver_->request_ = NULL;

    callback_->Run(error_);
  }

  void Cancel() {
    resolver_ = NULL;

    AutoLock locked(origin_loop_lock_);
    origin_loop_ = NULL;
  }

 private:
  // Set on the origin thread, read on the worker thread.
  std::string host_;
  std::string port_;

  // Only used on the origin thread (where Resolve was called).
  HostResolver* resolver_;
  AddressList* addresses_;
  CompletionCallback* callback_;

  // Used to post ourselves onto the origin thread.
  Lock origin_loop_lock_;
  MessageLoop* origin_loop_;

  // Assigned on the worker thread, read on the origin thread.
  int error_;
  struct addrinfo* results_;
};

//-----------------------------------------------------------------------------

HostResolver::HostResolver() {
#if defined(OS_WIN)
  EnsureWinsockInit();
#endif
}

HostResolver::~HostResolver() {
  if (request_)
    request_->Cancel();
}

int HostResolver::Resolve(const std::string& hostname, int port,
                          AddressList* addresses,
                          CompletionCallback* callback) {
  DCHECK(!request_) << "resolver already in use";

  const std::string& port_str = IntToString(port);

  // Do a synchronous resolution.
  if (!callback) {
    struct addrinfo* results;
    int rv = ResolveAddrInfo(hostname, port_str, &results);
    if (rv == OK)
      addresses->Adopt(results);
    return rv;
  }

  request_ = new Request(this, hostname, port_str, addresses, callback);

  // Dispatch to worker thread...
  if (!WorkerPool::PostTask(FROM_HERE,
          NewRunnableMethod(request_.get(), &Request::DoLookup), true)) {
    NOTREACHED();
    request_ = NULL;
    return ERR_FAILED;
  }

  return ERR_IO_PENDING;
}

}  // namespace net
