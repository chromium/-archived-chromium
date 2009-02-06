// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

#ifndef CHROME_BROWSER_NET_DNS_SLAVE_H_
#define CHROME_BROWSER_NET_DNS_SLAVE_H_

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
  DnsSlave(DnsMaster* master, size_t slave_index)
      : master_(master),
        slave_index_(slave_index) {
  }

  ~DnsSlave() {
    master_ = NULL;
  }

  static DWORD WINAPI ThreadStart(void* pThis);

  unsigned Run();

 private:
  std::string hostname_;  // Name being looked up.

  DnsMaster* master_;  // Master that started us.
  size_t slave_index_;  // Our index into DnsMaster's array.

  void BlockingDnsLookup();

  DISALLOW_COPY_AND_ASSIGN(DnsSlave);
};

}  // namespace chrome_browser_net

#endif  // CHROME_BROWSER_NET_DNS_SLAVE_H_

