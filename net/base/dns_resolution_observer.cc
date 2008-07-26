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

// This file supports network stack independent notification of progress
// towards resolving a hostname.

#include "net/base/dns_resolution_observer.h"

#include <string>

#include "base/atomic.h"
#include "base/logging.h"

namespace net {

static DnsResolutionObserver* dns_resolution_observer;

void AddDnsResolutionObserver(DnsResolutionObserver* new_observer) {
  if (new_observer == dns_resolution_observer)
    return;  // Facilitate unit tests that init/teardown repeatedly.
  DCHECK(!dns_resolution_observer);
  if (InterlockedCompareExchangePointer(
          reinterpret_cast<PVOID*>(&dns_resolution_observer),
          new_observer, NULL))
    DCHECK(0) << "Second attempt to setup observer";
}

DnsResolutionObserver* RemoveDnsResolutionObserver() {
  // We really need to check that the entire network subsystem is shutting down,
  // and hence no additional calls can even *possibly* still be lingering in the
  // notification path that includes our observer.  Until we have a way to
  // really assert that fact, we will outlaw the calling of this function.
  // Darin suggested that the caller use a static initializer for the observer,
  // so that it can safely be destroyed after process termination, and without
  // inducing a memory leak.
  // Bottom line: Don't call this function!  You will crash for now.
  CHECK(0);
  DnsResolutionObserver* old_observer = dns_resolution_observer;
  dns_resolution_observer = NULL;
  return old_observer;
}

// Locking access to dns_resolution_observer is not really critical... but we
// should test the value of dns_resolution_observer that we use.
// Worst case, we'll get an "out of date" value... which is no big deal for the
// DNS prefetching system (the most common (only?) observer).
void DidStartDnsResolution(const std::string& name, void* context) {
  DnsResolutionObserver* current_observer = dns_resolution_observer;
  if (current_observer)
    current_observer->OnStartResolution(name, context);
}

void DidFinishDnsResolutionWithStatus(bool was_resolved, void* context) {
  DnsResolutionObserver* current_observer = dns_resolution_observer;
  if (current_observer) {
    current_observer->OnFinishResolutionWithStatus(was_resolved, context);
  }
}
}  // namspace net
