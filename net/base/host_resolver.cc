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

#include "net/base/host_resolver.h"

#include <ws2tcpip.h>
#include <wspiapi.h>  // Needed for Win2k compat.

#include "base/string_util.h"
#include "net/base/address_list.h"
#include "net/base/net_errors.h"
#include "net/base/winsock_init.h"

namespace net {

//-----------------------------------------------------------------------------

static int ResolveAddrInfo(const std::string& host, const std::string& port,
                           struct addrinfo** results) {
  struct addrinfo hints = {0};
  hints.ai_family = PF_UNSPEC;
  hints.ai_flags = AI_ADDRCONFIG;

  // Restrict result set to only this socket type to avoid duplicates.
  hints.ai_socktype = SOCK_STREAM;

  int err = getaddrinfo(host.c_str(), port.c_str(), &hints, results);
  return err ? ERR_NAME_NOT_RESOLVED : OK;
}

//-----------------------------------------------------------------------------

struct HostResolver::Request :
    public base::RefCountedThreadSafe<HostResolver::Request> {
  Request() : error(OK), results(NULL) {
    DuplicateHandle(GetCurrentProcess(), GetCurrentThread(),
                    GetCurrentProcess(), &origin_thread,
                    0, FALSE, DUPLICATE_SAME_ACCESS);
  }
  ~Request() {
    CloseHandle(origin_thread);
  }

  // Only used on the origin thread (where Resolve was called).
  AddressList* addresses;
  CompletionCallback* callback;

  // Set on the origin thread, read on the worker thread.
  std::string host;
  std::string port;
  HANDLE origin_thread;

  // Assigned on the worker thread.
  int error;
  struct addrinfo* results;

  static void CALLBACK ReturnResults(ULONG_PTR param) {
    Request* r = reinterpret_cast<Request*>(param);
    // The HostResolver may have gone away.
    if (r->addresses) {
      DCHECK(r->addresses);
      r->addresses->Adopt(r->results);
      if (r->callback)
        r->callback->Run(r->error);
    } else if (r->results) {
      freeaddrinfo(r->results);
    }
    r->Release();
  }

  static DWORD CALLBACK DoLookup(void* param) {
    Request* r = static_cast<Request*>(param);

    r->error = ResolveAddrInfo(r->host, r->port, &r->results);

    if (!QueueUserAPC(ReturnResults, r->origin_thread,
                      reinterpret_cast<ULONG_PTR>(param))) {
      // The origin thread must have gone away.
      if (r->results)
        freeaddrinfo(r->results);
      r->Release();
    }
    return 0;
  }
};

//-----------------------------------------------------------------------------

HostResolver::HostResolver() {
  EnsureWinsockInit();
}

HostResolver::~HostResolver() {
  if (request_) {
    request_->addresses = NULL;
    request_->callback = NULL;
  }
}

int HostResolver::Resolve(const std::string& hostname, int port,
                          AddressList* addresses,
                          CompletionCallback* callback) {
  DCHECK(!request_);

  const std::string& port_str = IntToString(port);

  // Do a synchronous resolution?
  if (!callback) {
    struct addrinfo* results;
    int rv = ResolveAddrInfo(hostname, port_str, &results);
    if (rv == OK)
      addresses->Adopt(results);
    return rv;
  }

  // Dispatch to worker thread...
  request_ = new Request();
  request_->host = hostname;
  request_->port = port_str;
  request_->addresses = addresses;
  request_->callback = callback;

  // Balanced in Request::ReturnResults (or DoLookup if there is an error).
  request_->AddRef();
  if (!QueueUserWorkItem(Request::DoLookup, request_, WT_EXECUTELONGFUNCTION)) {
    DLOG(ERROR) << "QueueUserWorkItem failed: " << GetLastError();
    request_->Release();
    request_ = NULL;
    return ERR_FAILED;
  }

  return ERR_IO_PENDING;
}

}  // namespace net
