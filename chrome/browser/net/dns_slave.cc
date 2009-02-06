// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// See header file for description of class

#include <ws2tcpip.h>
#include <Wspiapi.h>  // Needed for win2k compatibility

#include "chrome/browser/net/dns_slave.h"

#include "base/logging.h"
#include "base/platform_thread.h"
#include "base/string_util.h"
#include "chrome/browser/net/dns_host_info.h"
#include "chrome/browser/net/dns_master.h"


namespace chrome_browser_net {

//------------------------------------------------------------------------------
// We supply a functions for stubbing network callbacks, so that offline testing
// can be performed.
//------------------------------------------------------------------------------
static GetAddrInfoFunction get_addr = getaddrinfo;
static FreeAddrInfoFunction free_addr = freeaddrinfo;
void SetAddrinfoCallbacks(GetAddrInfoFunction getaddrinfo,
                               FreeAddrInfoFunction freeaddrinfo) {
  get_addr = getaddrinfo;
  free_addr = freeaddrinfo;
}

GetAddrInfoFunction get_getaddrinfo() {return get_addr;}
FreeAddrInfoFunction get_freeaddrinfo() {return free_addr;}


//------------------------------------------------------------------------------
// This is the entry method used by DnsMaster to start our thread.
//------------------------------------------------------------------------------

DWORD __stdcall DnsSlave::ThreadStart(void* pThis) {
  DnsSlave* slave_instance = reinterpret_cast<DnsSlave*>(pThis);
  return slave_instance->Run();
}

//------------------------------------------------------------------------------
// The following are methods on the DnsPrefetch class.
//------------------------------------------------------------------------------

unsigned DnsSlave::Run() {
  DCHECK(slave_index_ >= 0 && slave_index_ < DnsMaster::kSlaveCountMax);

  std::string name = StringPrintf(
      "dns_prefetcher_%d_of_%d", slave_index_ + 1, DnsMaster::kSlaveCountMax);
  DLOG(INFO) << "Now Running " << name;
  PlatformThread::SetName(name.c_str());

  while (master_->GetNextAssignment(&hostname_)) {
    BlockingDnsLookup();
  }
  // GetNextAssignment() fails when we are told to terminate.
  master_->SetSlaveHasTerminated(slave_index_);
  return 0;
}

void DnsSlave::BlockingDnsLookup() {
  const char* port = "80";  // I may need to get the real port
  addrinfo* result = NULL;

  int error_code = get_addr(hostname_.c_str(), port, NULL, &result);

  // Note that since info has value semantics, I need to ask
  // master_ to set the new states atomically in its map.
  switch (error_code) {
    case 0:
      master_->SetFoundState(hostname_);
      break;

    default:
      DCHECK_EQ(0, error_code) << "surprising output" ;
      // fall through

    case WSAHOST_NOT_FOUND:
      master_->SetNoSuchNameState(hostname_);
      break;
  }

  // We don't save results, so lets free them...
  if (result) {
    free_addr(result);
    result = NULL;
  }
}

}  // namespace chrome_browser_net

