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

// The observer class supports exactly one active (Add'ed) instance, and in
// typical usage, that observer will be Add'ed during process startup, and
// Remove'd during process termination.


#ifndef NET_BASE_DNS_RESOLUTION_OBSERVER_H__
#define NET_BASE_DNS_RESOLUTION_OBSERVER_H__

#include <string>

namespace net {

class DnsResolutionObserver {
 public:
  // For each OnStartResolution() notification, there should be a later
  // OnFinishResolutionWithStatus() indicating completion of the resolution
  // activity.
  // Related pairs of notification will arrive with matching context values.
  // A caller may use the context values to match up these asynchronous calls
  // from among a larger call stream.
  // Once a matching pair of notifications has been provided (i.e., a pair with
  // identical context values), and the notification methods (below) have
  // returned, the context values *might* be reused.
  virtual void OnStartResolution(const std::string& name, void* context) = 0;
  virtual void OnFinishResolutionWithStatus(bool was_resolved,
                                            void* context) = 0;
};


// Note that *exactly* one observer is currently supported, and any attempt to
// add a second observer via AddDnsResolutionObserver() before removing the
// first DnsResolutionObserver will induce a DCHECK() assertion.
void AddDnsResolutionObserver(DnsResolutionObserver* new_observer);

// Note that the RemoveDnsResolutionObserver() will NOT perform any delete
// operations, and it is the responsibility of the code that called
// AddDnsResolutionObserver() to make a corresponding call to
// RemoveDnsResolutionObserver() and then delete the returned
// DnsResolutionObserver instance.
DnsResolutionObserver* RemoveDnsResolutionObserver();

// The following functions are expected to be called only by network stack
// implementations.  This above observer class will relay the notifications
// to any registered observer.
void DidStartDnsResolution(const std::string& name, void* context);
void DidFinishDnsResolutionWithStatus(bool was_resolved, void* context);
}  // namspace net
#endif  // NET_BASE_DNS_RESOLUTION_OBSERVER_H__
