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
#if defined(OS_LINUX)
#include <resolv.h>
#endif

#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/worker_pool.h"
#include "net/base/address_list.h"
#include "net/base/net_errors.h"

#if defined(OS_LINUX)
#include "base/singleton.h"
#include "base/thread_local_storage.h"
#include "base/time.h"
#endif

#if defined(OS_WIN)
#include "net/base/winsock_init.h"
#endif

namespace net {

//-----------------------------------------------------------------------------

static HostMapper* host_mapper;

std::string HostMapper::MapUsingPrevious(const std::string& host) {
  return previous_mapper_.get() ? previous_mapper_->Map(host) : host;
}

HostMapper* SetHostMapper(HostMapper* value) {
  std::swap(host_mapper, value);
  return value;
}

#if defined(OS_LINUX)
// On Linux changes to /etc/resolv.conf can go unnoticed thus resulting in
// DNS queries failing either because nameservers are unknown on startup
// or because nameserver info has changed as a result of e.g. connecting to
// a new network. Some distributions patch glibc to stat /etc/resolv.conf
// to try to automatically detect such changes but these patches are not
// universal and even patched systems such as Jaunty appear to need calls
// to res_ninit to reload the nameserver information in different threads.
//
// We adopt the Mozilla solution here which is to call res_ninit when
// lookups fail and to rate limit the reloading to once per second per
// thread.

// Keep a timer per calling thread to rate limit the calling of res_ninit.
class DnsReloadTimer {
 public:
  DnsReloadTimer() {
    tls_index_.Initialize(SlotReturnFunction);
  }

  ~DnsReloadTimer() { }

  // Check if the timer for the calling thread has expired. When no
  // timer exists for the calling thread, create one.
  bool Expired() {
    const base::TimeDelta kRetryTime = base::TimeDelta::FromSeconds(1);
    base::TimeTicks now = base::TimeTicks::Now();
    base::TimeTicks* timer_ptr =
      static_cast<base::TimeTicks*>(tls_index_.Get());

    if (!timer_ptr) {
      timer_ptr = new base::TimeTicks();
      *timer_ptr = base::TimeTicks::Now();
      tls_index_.Set(timer_ptr);
      // Return true to reload dns info on the first call for each thread.
      return true;
    } else if (now - *timer_ptr > kRetryTime) {
      *timer_ptr = now;
      return true;
    } else {
      return false;
    }
  }

  // Free the allocated timer.
  static void SlotReturnFunction(void* data) {
    base::TimeTicks* tls_data = static_cast<base::TimeTicks*>(data);
    delete tls_data;
  }

 private:
  // We use thread local storage to identify which base::TimeTicks to
  // interact with.
  static ThreadLocalStorage::Slot tls_index_ ;

  DISALLOW_COPY_AND_ASSIGN(DnsReloadTimer);
};

// A TLS slot to the TimeTicks for the current thread.
// static
ThreadLocalStorage::Slot DnsReloadTimer::tls_index_(base::LINKER_INITIALIZED);

#endif  // defined(OS_LINUX)

static int HostResolverProc(
    const std::string& host, const std::string& port, struct addrinfo** out) {
  struct addrinfo hints = {0};
  hints.ai_family = AF_UNSPEC;

#if defined(OS_WIN)
  // DO NOT USE AI_ADDRCONFIG ON WINDOWS.
  //
  // The following comment in <winsock2.h> is the best documentation I found
  // on AI_ADDRCONFIG for Windows:
  //   Flags used in "hints" argument to getaddrinfo()
  //       - AI_ADDRCONFIG is supported starting with Vista
  //       - default is AI_ADDRCONFIG ON whether the flag is set or not
  //         because the performance penalty in not having ADDRCONFIG in
  //         the multi-protocol stack environment is severe;
  //         this defaulting may be disabled by specifying the AI_ALL flag,
  //         in that case AI_ADDRCONFIG must be EXPLICITLY specified to
  //         enable ADDRCONFIG behavior
  //
  // Not only is AI_ADDRCONFIG unnecessary, but it can be harmful.  If the
  // computer is not connected to a network, AI_ADDRCONFIG causes getaddrinfo
  // to fail with WSANO_DATA (11004) for "localhost", probably because of the
  // following note on AI_ADDRCONFIG in the MSDN getaddrinfo page:
  //   The IPv4 or IPv6 loopback address is not considered a valid global
  //   address.
  // See http://crbug.com/5234.
  hints.ai_flags = 0;
#else
  hints.ai_flags = AI_ADDRCONFIG;
#endif

  // Restrict result set to only this socket type to avoid duplicates.
  hints.ai_socktype = SOCK_STREAM;

  int err = getaddrinfo(host.c_str(), port.c_str(), &hints, out);
#if defined(OS_LINUX)
  net::DnsReloadTimer* dns_timer = Singleton<net::DnsReloadTimer>::get();
  // If we fail, re-initialise the resolver just in case there have been any
  // changes to /etc/resolv.conf and retry. See http://crbug.com/11380 for info.
  if (err && dns_timer->Expired() && !res_ninit(&_res))
    err = getaddrinfo(host.c_str(), port.c_str(), &hints, out);
#endif

  return err ? ERR_NAME_NOT_RESOLVED : OK;
}

static int ResolveAddrInfo(HostMapper* mapper, const std::string& host,
                           const std::string& port, struct addrinfo** out) {
  if (mapper) {
    std::string mapped_host = mapper->Map(host);

    if (mapped_host.empty())
      return ERR_NAME_NOT_RESOLVED;

    return HostResolverProc(mapped_host, port, out);
  } else {
    return HostResolverProc(host, port, out);
  }
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
        host_mapper_(host_mapper),
        error_(OK),
        results_(NULL) {
  }

  ~Request() {
    if (results_)
      freeaddrinfo(results_);
  }

  void DoLookup() {
    // Running on the worker thread
    error_ = ResolveAddrInfo(host_mapper_, host_, port_, &results_);

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

  // Hold an owning reference to the host mapper that we are going to use.
  // This may not be the current host mapper by the time we call
  // ResolveAddrInfo, but that's OK... we'll use it anyways, and the owning
  // reference ensures that it remains valid until we are done.
  scoped_refptr<HostMapper> host_mapper_;

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
    int rv = ResolveAddrInfo(host_mapper, hostname, port_str, &results);
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
