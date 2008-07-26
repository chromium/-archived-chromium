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

// A DnsSlave object processes hostname lookups
// via DNS on a single thread, waiting for that blocking
// call to complete, and then getting its next hostname
// from its associated DnsMaster object.
// Since this class only is concerned with prefetching
// to warm the underlying DNS cache, the actual IP
// does not need to be recorded.  It is necessary to record
// when the lookup finished, so that the associated DnsMaster
// won't (wastefully) ask for the same name in "too short a
// period of time."
// This class does no "de-duping," and merely slavishly services
// items supplied by its DnsMaster.

#ifndef CHROME_BROWSER_NET_DNS_SLAVE_H__
#define CHROME_BROWSER_NET_DNS_SLAVE_H__

#include <windows.h>
#include <string>

#include "base/basictypes.h"

namespace chrome_browser_net {

class DnsMaster;
class DnsSlave;

// Support testing infrastructure, and allow off-line testing.
typedef void (__stdcall *FreeAddrInfoFunction)(struct addrinfo* ai);
typedef int (__stdcall *GetAddrInfoFunction)(const char* nodename,
                                             const char* servname,
                                             const struct addrinfo* hints,
                                             struct addrinfo** result);
void SetAddrinfoCallbacks(GetAddrInfoFunction getaddrinfo,
                          FreeAddrInfoFunction freeaddrinfo);

GetAddrInfoFunction get_getaddrinfo();
FreeAddrInfoFunction get_freeaddrinfo();

class DnsSlave {
 public:
  DnsSlave(DnsMaster* master, int slave_index)
      : slave_index_(slave_index),
        master_(master) {
  }

  ~DnsSlave() {
    master_ = NULL;
  }

  static DWORD WINAPI ThreadStart(void* pThis);

  unsigned Run();

 private:
  std::string hostname_;  // Name being looked up.

  DnsMaster* master_;  // Master that started us.
  int slave_index_;  // Our index into DnsMaster's array.

  void BlockingDnsLookup();

  DISALLOW_EVIL_CONSTRUCTORS(DnsSlave);
};

}  // namespace chrome_browser_net

#endif  // CHROME_BROWSER_NET_DNS_SLAVE_H__
